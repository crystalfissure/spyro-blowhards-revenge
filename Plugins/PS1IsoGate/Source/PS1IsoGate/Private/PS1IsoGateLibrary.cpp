#include "PS1IsoGateLibrary.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PS1IsoGateSettings.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    constexpr int32 MarkerScanChunkSize = 1024 * 1024;

    FString NormalizeSha256(const FString& Hash)
    {
        FString Normalized = Hash.TrimStartAndEnd().ToLower();
        Normalized.ReplaceInline(TEXT(" "), TEXT(""));
        return Normalized;
    }

    bool IsHexSha256(const FString& Hash)
    {
        if (Hash.Len() != 64)
        {
            return false;
        }

        for (const TCHAR Character : Hash)
        {
            const bool bIsHex =
                (Character >= TCHAR('0') && Character <= TCHAR('9')) ||
                (Character >= TCHAR('a') && Character <= TCHAR('f'));
            if (!bIsHex)
            {
                return false;
            }
        }

        return true;
    }

    FORCEINLINE uint32 RotateRight(uint32 Value, uint32 Bits)
    {
        return (Value >> Bits) | (Value << (32 - Bits));
    }

    class FSha256Context
    {
    public:
        void Update(const uint8* Data, uint64 Size)
        {
            BitLength += Size * 8ULL;

            uint64 Offset = 0;
            if (BufferSize > 0)
            {
                const uint64 BytesToCopy = FMath::Min<uint64>(64 - BufferSize, Size);
                FMemory::Memcpy(Buffer + BufferSize, Data, BytesToCopy);
                BufferSize += BytesToCopy;
                Offset += BytesToCopy;

                if (BufferSize == 64)
                {
                    Transform(Buffer);
                    BufferSize = 0;
                }
            }

            while (Offset + 64 <= Size)
            {
                Transform(Data + Offset);
                Offset += 64;
            }

            if (Offset < Size)
            {
                BufferSize = Size - Offset;
                FMemory::Memcpy(Buffer, Data + Offset, BufferSize);
            }
        }

        FString Final()
        {
            Buffer[BufferSize++] = 0x80;

            if (BufferSize > 56)
            {
                while (BufferSize < 64)
                {
                    Buffer[BufferSize++] = 0;
                }
                Transform(Buffer);
                BufferSize = 0;
            }

            while (BufferSize < 56)
            {
                Buffer[BufferSize++] = 0;
            }

            for (int32 Shift = 56; Shift >= 0; Shift -= 8)
            {
                Buffer[BufferSize++] = static_cast<uint8>((BitLength >> Shift) & 0xff);
            }
            Transform(Buffer);

            FString Hash;
            Hash.Reserve(64);
            for (uint32 Word : H)
            {
                Hash += FString::Printf(TEXT("%08x"), Word);
            }
            return Hash;
        }

    private:
        void Transform(const uint8* Block)
        {
            static const uint32 K[64] =
            {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            };

            uint32 W[64] = {};
            for (int32 Index = 0; Index < 16; ++Index)
            {
                const int32 ByteIndex = Index * 4;
                W[Index] =
                    (static_cast<uint32>(Block[ByteIndex]) << 24) |
                    (static_cast<uint32>(Block[ByteIndex + 1]) << 16) |
                    (static_cast<uint32>(Block[ByteIndex + 2]) << 8) |
                    static_cast<uint32>(Block[ByteIndex + 3]);
            }

            for (int32 Index = 16; Index < 64; ++Index)
            {
                const uint32 S0 = RotateRight(W[Index - 15], 7) ^ RotateRight(W[Index - 15], 18) ^ (W[Index - 15] >> 3);
                const uint32 S1 = RotateRight(W[Index - 2], 17) ^ RotateRight(W[Index - 2], 19) ^ (W[Index - 2] >> 10);
                W[Index] = W[Index - 16] + S0 + W[Index - 7] + S1;
            }

            uint32 A = H[0];
            uint32 B = H[1];
            uint32 C = H[2];
            uint32 D = H[3];
            uint32 E = H[4];
            uint32 F = H[5];
            uint32 G = H[6];
            uint32 I = H[7];

            for (int32 Index = 0; Index < 64; ++Index)
            {
                const uint32 S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
                const uint32 Ch = (E & F) ^ ((~E) & G);
                const uint32 Temp1 = I + S1 + Ch + K[Index] + W[Index];
                const uint32 S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
                const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
                const uint32 Temp2 = S0 + Maj;

                I = G;
                G = F;
                F = E;
                E = D + Temp1;
                D = C;
                C = B;
                B = A;
                A = Temp1 + Temp2;
            }

            H[0] += A;
            H[1] += B;
            H[2] += C;
            H[3] += D;
            H[4] += E;
            H[5] += F;
            H[6] += G;
            H[7] += I;
        }

        uint32 H[8] =
        {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
        uint8 Buffer[64] = {};
        uint64 BufferSize = 0;
        uint64 BitLength = 0;
    };

    FString ExpandPath(const FString& Path)
    {
        FString ExpandedPath = Path.TrimStartAndEnd();
        FPaths::NormalizeFilename(ExpandedPath);
        return FPaths::ConvertRelativePathToFull(ExpandedPath);
    }

    FString ResolveCueDataFile(const FString& CuePath)
    {
        FString CueText;
        if (!FFileHelper::LoadFileToString(CueText, *CuePath))
        {
            return FString();
        }

        TArray<FString> Lines;
        CueText.ParseIntoArrayLines(Lines, true);

        for (FString Line : Lines)
        {
            Line = Line.TrimStartAndEnd();
            if (!Line.StartsWith(TEXT("FILE"), ESearchCase::IgnoreCase))
            {
                continue;
            }

            FString DataFile;
            int32 FirstQuote = INDEX_NONE;
            int32 SecondQuote = INDEX_NONE;
            if (Line.FindChar(TCHAR('"'), FirstQuote) && Line.FindLastChar(TCHAR('"'), SecondQuote) && SecondQuote > FirstQuote)
            {
                DataFile = Line.Mid(FirstQuote + 1, SecondQuote - FirstQuote - 1);
            }
            else
            {
                FString Remainder = Line.RightChop(4).TrimStartAndEnd();
                int32 SpaceIndex = INDEX_NONE;
                if (Remainder.FindChar(TCHAR(' '), SpaceIndex))
                {
                    DataFile = Remainder.Left(SpaceIndex).TrimStartAndEnd();
                }
                else
                {
                    DataFile = Remainder;
                }
            }

            if (!DataFile.IsEmpty())
            {
                FPaths::NormalizeFilename(DataFile);
                return FPaths::IsRelative(DataFile)
                    ? FPaths::ConvertRelativePathToFull(FPaths::GetPath(CuePath), DataFile)
                    : DataFile;
            }
        }

        return FString();
    }

    uint8 ToUpperByte(uint8 Value)
    {
        return Value >= 'a' && Value <= 'z' ? Value - 32 : Value;
    }

    TArray<uint8> ToUpperAsciiBytes(const FString& Text)
    {
        TArray<uint8> Bytes;
        const FTCHARToUTF8 Utf8(*Text.ToUpper());
        Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
        return Bytes;
    }

    bool ContainsNeedle(const TArray<uint8>& Haystack, const TArray<uint8>& Needle)
    {
        if (Needle.Num() == 0 || Haystack.Num() < Needle.Num())
        {
            return false;
        }

        for (int32 Index = 0; Index <= Haystack.Num() - Needle.Num(); ++Index)
        {
            bool bMatches = true;
            for (int32 NeedleIndex = 0; NeedleIndex < Needle.Num(); ++NeedleIndex)
            {
                if (ToUpperByte(Haystack[Index + NeedleIndex]) != Needle[NeedleIndex])
                {
                    bMatches = false;
                    break;
                }
            }

            if (bMatches)
            {
                return true;
            }
        }

        return false;
    }

    bool ScanFileForMarkers(const FString& FilePath, const TArray<FString>& Markers, TArray<FString>& MissingMarkers, FString& OutError)
    {
        TArray<TArray<uint8>> Needles;
        TArray<FString> NeedleLabels;
        int32 MaxNeedleSize = 0;
        for (const FString& Marker : Markers)
        {
            TArray<uint8> Needle = ToUpperAsciiBytes(Marker);
            if (Needle.Num() > 0)
            {
                MaxNeedleSize = FMath::Max(MaxNeedleSize, Needle.Num());
                NeedleLabels.Add(Marker);
                Needles.Add(Needle);
            }
        }

        TArray<bool> Found;
        Found.Init(false, Needles.Num());

        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*FilePath));
        if (!FileHandle)
        {
            OutError = FString::Printf(TEXT("Could not read disc image data: %s"), *FilePath);
            return false;
        }

        TArray<uint8> Chunk;
        TArray<uint8> Window;
        TArray<uint8> Tail;
        Chunk.SetNumUninitialized(MarkerScanChunkSize);

        while (true)
        {
            const int64 CurrentOffset = FileHandle->Tell();
            const int64 FileSize = FileHandle->Size();
            if (CurrentOffset >= FileSize)
            {
                break;
            }

            const int32 BytesToRead = static_cast<int32>(FMath::Min<int64>(MarkerScanChunkSize, FileSize - CurrentOffset));
            if (!FileHandle->Read(Chunk.GetData(), BytesToRead))
            {
                OutError = FString::Printf(TEXT("Could not continue reading disc image data: %s"), *FilePath);
                return false;
            }

            Window.Reset(Tail.Num() + BytesToRead);
            Window.Append(Tail);
            Window.Append(Chunk.GetData(), BytesToRead);

            for (int32 Index = 0; Index < Needles.Num(); ++Index)
            {
                if (!Found[Index] && ContainsNeedle(Window, Needles[Index]))
                {
                    Found[Index] = true;
                }
            }

            bool bAllFound = true;
            for (bool bFound : Found)
            {
                if (!bFound)
                {
                    bAllFound = false;
                    break;
                }
            }
            if (bAllFound)
            {
                return true;
            }

            const int32 TailSize = FMath::Min(FMath::Max(MaxNeedleSize - 1, 0), Window.Num());
            Tail.Reset(TailSize);
            if (TailSize > 0)
            {
                Tail.Append(Window.GetData() + Window.Num() - TailSize, TailSize);
            }
        }

        for (int32 Index = 0; Index < Found.Num(); ++Index)
        {
            if (!Found[Index])
            {
                MissingMarkers.Add(NeedleLabels[Index]);
            }
        }

        return true;
    }

    bool HashFileSha256(const FString& FilePath, FString& OutHash, FString& OutError)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*FilePath));
        if (!FileHandle)
        {
            OutError = FString::Printf(TEXT("Could not read disc image data: %s"), *FilePath);
            return false;
        }

        FSha256Context Sha256;
        TArray<uint8> Chunk;
        Chunk.SetNumUninitialized(MarkerScanChunkSize);

        while (true)
        {
            const int64 CurrentOffset = FileHandle->Tell();
            const int64 FileSize = FileHandle->Size();
            if (CurrentOffset >= FileSize)
            {
                break;
            }

            const int32 BytesToRead = static_cast<int32>(FMath::Min<int64>(MarkerScanChunkSize, FileSize - CurrentOffset));
            if (!FileHandle->Read(Chunk.GetData(), BytesToRead))
            {
                OutError = FString::Printf(TEXT("Could not continue hashing disc image data: %s"), *FilePath);
                return false;
            }

            Sha256.Update(Chunk.GetData(), BytesToRead);
        }

        OutHash = Sha256.Final();
        return true;
    }

    bool OpenWindowsDiscImageDialog(FString& SelectedDiscImagePath)
    {
#if PLATFORM_WINDOWS
        TCHAR FileName[MAX_PATH] = {};

        OPENFILENAME OpenFileName = {};
        OpenFileName.lStructSize = sizeof(OPENFILENAME);
        OpenFileName.hwndOwner = nullptr;
        OpenFileName.lpstrFile = FileName;
        OpenFileName.nMaxFile = MAX_PATH;
        OpenFileName.lpstrFilter = TEXT("PS1 Disc Images\0*.iso;*.bin;*.cue\0ISO Files\0*.iso\0BIN Files\0*.bin\0CUE Files\0*.cue\0All Files\0*.*\0");
        OpenFileName.nFilterIndex = 1;
        OpenFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        OpenFileName.lpstrTitle = TEXT("Choose your PS1 disc image");

        if (!GetOpenFileName(&OpenFileName))
        {
            return false;
        }

        SelectedDiscImagePath = FString(FileName);
        FPaths::NormalizeFilename(SelectedDiscImagePath);
        return true;
#else
        SelectedDiscImagePath.Reset();
        return false;
#endif
    }
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::VerifyConfiguredPS1DiscImage()
{
    const UPS1IsoGateSettings* Settings = GetDefault<UPS1IsoGateSettings>();
    return VerifyPS1DiscImage(Settings->DiscImagePath, Settings->ExpectedSha256, Settings->AllowedExtensions, Settings->ExpectedBootExecutable, Settings->RequiredDiscFiles);
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::VerifyPS1DiscImage(const FString& DiscImagePath, const FString& ExpectedSha256, const TArray<FString>& AllowedExtensions, const FString& ExpectedBootExecutable, const TArray<FString>& RequiredDiscFiles)
{
    FPS1IsoVerificationResult Result;

    if (DiscImagePath.TrimStartAndEnd().IsEmpty())
    {
        Result.Message = TEXT("No disc image path is configured.");
        return Result;
    }

    const FString FullDiscImagePath = ExpandPath(DiscImagePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    Result.bExists = PlatformFile.FileExists(*FullDiscImagePath);
    if (!Result.bExists)
    {
        Result.Message = FString::Printf(TEXT("Disc image was not found: %s"), *FullDiscImagePath);
        return Result;
    }

    const FString Extension = FPaths::GetExtension(FullDiscImagePath, false).ToLower();
    for (const FString& AllowedExtension : AllowedExtensions)
    {
        if (Extension == AllowedExtension.ToLower())
        {
            Result.bAllowedExtension = true;
            break;
        }
    }

    if (!Result.bAllowedExtension)
    {
        Result.Message = FString::Printf(TEXT("Disc image extension '.%s' is not allowed."), *Extension);
        return Result;
    }

    FString DataPath = FullDiscImagePath;
    if (Extension == TEXT("cue"))
    {
        DataPath = ResolveCueDataFile(FullDiscImagePath);
        if (DataPath.IsEmpty() || !PlatformFile.FileExists(*DataPath))
        {
            Result.Message = TEXT("CUE file did not point to a readable BIN file.");
            return Result;
        }
    }

    const FString NormalizedExpectedHash = NormalizeSha256(ExpectedSha256);
    if (!NormalizedExpectedHash.IsEmpty() && !NormalizedExpectedHash.StartsWith(TEXT("replace_with")))
    {
        if (!IsHexSha256(NormalizedExpectedHash))
        {
            Result.Message = TEXT("Expected SHA-256 must be blank or a 64-character hexadecimal value.");
            return Result;
        }

        FString HashError;
        if (!HashFileSha256(DataPath, Result.ActualSha256, HashError))
        {
            Result.Message = HashError;
            return Result;
        }

        Result.bHashMatches = NormalizeSha256(Result.ActualSha256) == NormalizedExpectedHash;
        if (!Result.bHashMatches)
        {
            Result.Message = TEXT("Disc image was found, but its SHA-256 does not match the configured value.");
            return Result;
        }
    }
    else
    {
        Result.bHashMatches = true;
    }

    Result.BootExecutable = ExpectedBootExecutable.TrimStartAndEnd().ToUpper();

    TArray<FString> MarkersToFind;
    MarkersToFind.Add(TEXT("BOOT"));
    MarkersToFind.Add(Result.BootExecutable);
    for (const FString& RequiredDiscFile : RequiredDiscFiles)
    {
        MarkersToFind.Add(RequiredDiscFile);
    }

    FString ScanError;
    if (!ScanFileForMarkers(DataPath, MarkersToFind, Result.MissingFiles, ScanError))
    {
        Result.Message = ScanError;
        return Result;
    }

    if (Result.MissingFiles.Contains(TEXT("BOOT")) || Result.MissingFiles.Contains(Result.BootExecutable))
    {
        Result.Message = FString::Printf(TEXT("Disc image does not contain the expected PS1 boot executable '%s'."), *Result.BootExecutable);
        return Result;
    }

    if (Result.MissingFiles.Num() > 0)
    {
        Result.Message = FString::Printf(TEXT("Disc image is missing %d expected marker(s)."), Result.MissingFiles.Num());
        return Result;
    }

    Result.bCanPlay = true;
    Result.Message = TEXT("PS1 disc image verified. Access is enabled.");
    return Result;
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::VerifyPS1DiscImageWithConfiguredRules(const FString& DiscImagePath)
{
    const UPS1IsoGateSettings* Settings = GetDefault<UPS1IsoGateSettings>();
    return VerifyPS1DiscImage(DiscImagePath, Settings->ExpectedSha256, Settings->AllowedExtensions, Settings->ExpectedBootExecutable, Settings->RequiredDiscFiles);
}

bool UPS1IsoGateLibrary::ChoosePS1DiscImage(FString& SelectedDiscImagePath)
{
    return OpenWindowsDiscImageDialog(SelectedDiscImagePath);
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::ChooseAndVerifyConfiguredPS1DiscImage(FString& SelectedDiscImagePath)
{
    FPS1IsoVerificationResult Result;

    if (!ChoosePS1DiscImage(SelectedDiscImagePath))
    {
        Result.Message = TEXT("No PS1 disc image was selected.");
        return Result;
    }

    const UPS1IsoGateSettings* Settings = GetDefault<UPS1IsoGateSettings>();
    return VerifyPS1DiscImage(SelectedDiscImagePath, Settings->ExpectedSha256, Settings->AllowedExtensions, Settings->ExpectedBootExecutable, Settings->RequiredDiscFiles);
}
