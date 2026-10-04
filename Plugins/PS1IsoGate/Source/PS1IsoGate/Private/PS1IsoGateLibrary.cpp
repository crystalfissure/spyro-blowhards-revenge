#include "PS1IsoGateLibrary.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    constexpr int32 LogicalSectorSize = 2048;
    constexpr uint32 MaxRootDirectorySize = 1024 * 1024;
    constexpr uint32 MaxSystemCnfSize = 64 * 1024;

    struct FRequiredDiscEntry
    {
        const TCHAR* Name;
        bool bDirectory = false;
    };

    struct FDiscVariant
    {
        EPS1IsoGame Game;
        const TCHAR* BootExecutable;
        TArray<FRequiredDiscEntry> RequiredEntries;
    };

    // Root directory listings from Info/PS1IsoGate/PS1IsoGate Info.txt.
    // Each executable selects its own regional rules; files from different regions are never combined.
    const FDiscVariant DiscVariants[] =
    {
        { EPS1IsoGame::Spyro1, TEXT("SCUS_942.28"), {
            { TEXT("S0"), true }, { TEXT("SOURCE"), true },
            { TEXT("PETEXA0.STR") }, { TEXT("PETEXA1.STR") }, { TEXT("PETEXA2.STR") },
            { TEXT("PETEXA3.STR") }, { TEXT("PETEXA4.STR") }, { TEXT("PETEXA5.STR") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro1, TEXT("SCES_014.38"), {
            { TEXT("S0"), true }, { TEXT("SOURCE"), true },
            { TEXT("MUSIC1.STR") }, { TEXT("MUSIC2.STR") }, { TEXT("MUSIC3.STR") },
            { TEXT("MUSIC4.STR") }, { TEXT("MUSIC5.STR") }, { TEXT("MUSIC6.STR") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro1, TEXT("SCPS_100.85"), {
            { TEXT("SOURCE"), true },
            { TEXT("MUSIC1.STR") }, { TEXT("MUSIC2.STR") }, { TEXT("MUSIC3.STR") },
            { TEXT("MUSIC4.STR") }, { TEXT("MUSIC5.STR") }, { TEXT("MUSIC6.STR") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro2, TEXT("SCUS_944.25"), {
            { TEXT("KART"), true }, { TEXT("SPEECH.STR") }, { TEXT("SPYRO2.TRD") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro2, TEXT("SCES_021.04"), {
            { TEXT("KART"), true }, { TEXT("SPEECH.STR") }, { TEXT("SPYRO2.TRD") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro2, TEXT("SCPS_101.28"), {
            { TEXT("SPEECH.STR") }, { TEXT("SPYRO2.TRD") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro3, TEXT("SCUS_944.67"), {
            { TEXT("CRASHBSH"), true }, { TEXT("3MN_BLNK.DAT") }, { TEXT("SPEECH.STR") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } },
        { EPS1IsoGame::Spyro3, TEXT("SCES_028.35"), {
            { TEXT("CRASHBSH"), true }, { TEXT("SPEECH.STR") }, { TEXT("SPYRO3.TRD") },
            { TEXT("SYSTEM.CNF") }, { TEXT("WAD.WAD") } } }
    };

    bool IsSupportedGame(EPS1IsoGame Game)
    {
        return Game == EPS1IsoGame::Spyro1 || Game == EPS1IsoGame::Spyro2 || Game == EPS1IsoGame::Spyro3;
    }

    FString GetGameName(EPS1IsoGame Game)
    {
        return FString::Printf(TEXT("Spyro %d"), static_cast<int32>(Game) + 1);
    }

    FString ExpandPath(const FString& Path)
    {
        FString ExpandedPath = Path.TrimStartAndEnd();
        FPaths::NormalizeFilename(ExpandedPath);
        return FPaths::ConvertRelativePathToFull(ExpandedPath);
    }

    struct FDiscDataSource
    {
        FString Path;
        int64 FirstSector = 0;
        int32 SectorSize = 0;
    };

    bool ResolveCueDataFile(const FString& CuePath, FDiscDataSource& OutSource)
    {
        FString CueText;
        if (!FFileHelper::LoadFileToString(CueText, *CuePath))
        {
            return false;
        }

        FString DataFile;
        bool bBinaryFile = false;
        bool bDataTrack = false;
        TArray<FString> Lines;
        CueText.ParseIntoArrayLines(Lines, true);
        for (const FString& UntrimmedLine : Lines)
        {
            const FString Line = UntrimmedLine.TrimStartAndEnd();
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() == 0)
            {
                continue;
            }

            if (Tokens[0].Equals(TEXT("FILE"), ESearchCase::IgnoreCase))
            {
                if (bDataTrack)
                {
                    return false; // The previous data track had no INDEX 01.
                }
                bBinaryFile = Tokens.Last().Equals(TEXT("BINARY"), ESearchCase::IgnoreCase);
                int32 FirstQuote = INDEX_NONE;
                int32 LastQuote = INDEX_NONE;
                if (Line.FindChar(TCHAR('"'), FirstQuote) && Line.FindLastChar(TCHAR('"'), LastQuote) && LastQuote > FirstQuote)
                {
                    DataFile = Line.Mid(FirstQuote + 1, LastQuote - FirstQuote - 1);
                }
                else
                {
                    DataFile = Tokens.Num() == 3 ? Tokens[1] : FString();
                }
            }
            else if (Tokens[0].Equals(TEXT("TRACK"), ESearchCase::IgnoreCase) && Tokens.Num() == 3)
            {
                if (bDataTrack)
                {
                    return false;
                }
                const FString Mode = Tokens[2].ToUpper();
                bDataTrack = Mode.StartsWith(TEXT("MODE1/")) || Mode.StartsWith(TEXT("MODE2/"));
                if (bDataTrack)
                {
                    OutSource.SectorSize = Mode == TEXT("MODE1/2048") ? 2048 :
                        (Mode == TEXT("MODE1/2352") || Mode == TEXT("MODE2/2352")) ? 2352 :
                        Mode == TEXT("MODE2/2336") ? 2336 : 0;
                    if (!bBinaryFile || DataFile.IsEmpty() || OutSource.SectorSize == 0)
                    {
                        return false;
                    }
                }
            }
            else if (bDataTrack && Tokens[0].Equals(TEXT("INDEX"), ESearchCase::IgnoreCase) &&
                Tokens.Num() == 3 && Tokens[1] == TEXT("01"))
            {
                TArray<FString> TimeParts;
                Tokens[2].ParseIntoArray(TimeParts, TEXT(":"), false);
                if (TimeParts.Num() != 3)
                {
                    return false;
                }
                for (const FString& Part : TimeParts)
                {
                    if (Part.IsEmpty() || Part.Len() > 3)
                    {
                        return false;
                    }
                    for (TCHAR Character : Part)
                    {
                        if (Character < TCHAR('0') || Character > TCHAR('9'))
                        {
                            return false;
                        }
                    }
                }
                const int32 Minutes = FCString::Atoi(*TimeParts[0]);
                const int32 Seconds = FCString::Atoi(*TimeParts[1]);
                const int32 Frames = FCString::Atoi(*TimeParts[2]);
                if (Seconds >= 60 || Frames >= 75)
                {
                    return false;
                }
                OutSource.FirstSector = (Minutes * 60 + Seconds) * 75 + Frames;
                FPaths::NormalizeFilename(DataFile);
                OutSource.Path = FPaths::IsRelative(DataFile)
                    ? FPaths::ConvertRelativePathToFull(FPaths::GetPath(CuePath), DataFile)
                    : DataFile;
                return true;
            }
        }
        return false;
    }

    uint32 ReadLittleEndian32(const uint8* Bytes)
    {
        return static_cast<uint32>(Bytes[0]) | (static_cast<uint32>(Bytes[1]) << 8) |
            (static_cast<uint32>(Bytes[2]) << 16) | (static_cast<uint32>(Bytes[3]) << 24);
    }

    struct FDiscEntry
    {
        uint32 Block = 0;
        uint32 Size = 0;
        bool bDirectory = false;
    };

    struct FDiscLayout
    {
        int32 SectorSize;
        int32 DataOffset;
    };

    class FDiscDirectoryReader
    {
    public:
        FDiscDirectoryReader(IFileHandle& InFile, const FDiscDataSource& InSource)
            : File(InFile), FirstSector(InSource.FirstSector), FileSize(InFile.Size())
        {
        }

        bool ReadRootDirectory(int32 SectorSizeHint, TMap<FString, FDiscEntry>& OutEntries, FString& OutError)
        {
            // Cooked ISO, raw Mode 1, raw Mode 2/XA, and Mode 2 without sync/header.
            const FDiscLayout Layouts[] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 }, { 2336, 8 } };
            uint8 Sector[LogicalSectorSize];
            bool bFoundPrimaryVolume = false;
            for (const FDiscLayout& Candidate : Layouts)
            {
                if (SectorSizeHint != 0 && Candidate.SectorSize != SectorSizeHint)
                {
                    continue;
                }
                Layout = Candidate;
                for (uint32 Block = 16; Block < 48; ++Block)
                {
                    if (!ReadBlock(Block, Sector, 7) || FMemory::Memcmp(Sector + 1, "CD001", 5) != 0 || Sector[6] != 1)
                    {
                        break;
                    }
                    if (Sector[0] == 255)
                    {
                        break;
                    }
                    if (Sector[0] == 1)
                    {
                        bFoundPrimaryVolume = ReadBlock(Block, Sector, LogicalSectorSize);
                        break;
                    }
                }
                if (bFoundPrimaryVolume)
                {
                    break;
                }
            }
            if (!bFoundPrimaryVolume || Sector[128] != 0 || Sector[129] != 8 ||
                Sector[156] < 34 || (Sector[181] & 2) == 0)
            {
                OutError = TEXT("Disc image does not contain a supported ISO 9660 data track.");
                return false;
            }

            const uint64 RootBlock = static_cast<uint64>(ReadLittleEndian32(Sector + 158)) + Sector[157];
            const uint32 RootSize = ReadLittleEndian32(Sector + 166);
            if (RootBlock > MAX_uint32 || RootSize == 0 || RootSize > MaxRootDirectorySize ||
                !IsExtentInImage(static_cast<uint32>(RootBlock), RootSize))
            {
                OutError = TEXT("Disc image has an invalid root directory.");
                return false;
            }

            for (uint32 Offset = 0; Offset < RootSize; Offset += LogicalSectorSize)
            {
                const int32 BytesToRead = static_cast<int32>(FMath::Min<uint32>(LogicalSectorSize, RootSize - Offset));
                if (!ReadBlock(static_cast<uint32>(RootBlock) + Offset / LogicalSectorSize, Sector, BytesToRead))
                {
                    OutError = TEXT("Could not read the disc image root directory.");
                    return false;
                }
                for (int32 Position = 0; Position < BytesToRead && Sector[Position] != 0;)
                {
                    const uint8* Record = Sector + Position;
                    const int32 RecordSize = Record[0];
                    if (RecordSize < 34 || RecordSize > BytesToRead - Position ||
                        Record[32] == 0 || 33 + Record[32] > RecordSize)
                    {
                        OutError = TEXT("Disc image contains a malformed directory entry.");
                        return false;
                    }
                    if (!(Record[32] == 1 && Record[33] <= 1))
                    {
                        FString Name;
                        for (int32 Index = 0; Index < Record[32] && Record[33 + Index] != ';'; ++Index)
                        {
                            Name.AppendChar(static_cast<TCHAR>(Record[33 + Index]));
                        }
                        const uint64 EntryBlock = static_cast<uint64>(ReadLittleEndian32(Record + 2)) + Record[1];
                        if (EntryBlock > MAX_uint32)
                        {
                            OutError = TEXT("Disc image contains an invalid file location.");
                            return false;
                        }
                        FDiscEntry Entry;
                        Entry.Block = static_cast<uint32>(EntryBlock);
                        Entry.Size = ReadLittleEndian32(Record + 10);
                        Entry.bDirectory = (Record[25] & 2) != 0;
                        OutEntries.Add(Name.ToUpper(), Entry);
                    }
                    Position += RecordSize;
                }
            }
            return true;
        }

        bool IsExtentInImage(uint32 Block, uint32 Size) const
        {
            if (Size == 0)
            {
                return false;
            }
            const int64 LastBlock = static_cast<int64>(Block) + (static_cast<int64>(Size) - 1) / LogicalSectorSize;
            const int64 End = (FirstSector + LastBlock) * Layout.SectorSize + Layout.DataOffset +
                (static_cast<int64>(Size) - 1) % LogicalSectorSize + 1;
            return End <= FileSize;
        }

        bool ReadSystemCnf(const FDiscEntry& Entry, FString& OutText)
        {
            if (Entry.bDirectory || Entry.Size == 0 || Entry.Size > MaxSystemCnfSize || !IsExtentInImage(Entry.Block, Entry.Size))
            {
                return false;
            }
            uint8 Sector[LogicalSectorSize];
            OutText.Reserve(Entry.Size);
            for (uint32 Offset = 0; Offset < Entry.Size; Offset += LogicalSectorSize)
            {
                const int32 BytesToRead = static_cast<int32>(FMath::Min<uint32>(LogicalSectorSize, Entry.Size - Offset));
                if (!ReadBlock(Entry.Block + Offset / LogicalSectorSize, Sector, BytesToRead))
                {
                    return false;
                }
                for (int32 Index = 0; Index < BytesToRead; ++Index)
                {
                    OutText.AppendChar(static_cast<TCHAR>(Sector[Index]));
                }
            }
            return true;
        }

    private:
        bool ReadBlock(uint32 Block, uint8* OutBytes, int32 Size)
        {
            const int64 Offset = (FirstSector + static_cast<int64>(Block)) * Layout.SectorSize + Layout.DataOffset;
            return Offset >= 0 && Offset <= FileSize - Size && File.Seek(Offset) && File.Read(OutBytes, Size);
        }

        IFileHandle& File;
        int64 FirstSector;
        int64 FileSize;
        FDiscLayout Layout = { 2048, 0 };
    };

    FString GetBootExecutable(const FString& SystemCnf)
    {
        TArray<FString> Lines;
        SystemCnf.ParseIntoArrayLines(Lines, true);
        for (const FString& Line : Lines)
        {
            FString Key;
            FString Value;
            if (Line.Split(TEXT("="), &Key, &Value) && Key.TrimStartAndEnd().Equals(TEXT("BOOT"), ESearchCase::IgnoreCase))
            {
                Value = Value.TrimStartAndEnd().ToUpper();
                if (!Value.StartsWith(TEXT("CDROM:")))
                {
                    return FString();
                }
                Value = Value.RightChop(6);
                Value.ReplaceInline(TEXT("\\"), TEXT("/"));
                while (Value.StartsWith(TEXT("/")))
                {
                    Value = Value.RightChop(1);
                }
                int32 VersionIndex = INDEX_NONE;
                if (Value.FindChar(TCHAR(';'), VersionIndex))
                {
                    Value = Value.Left(VersionIndex);
                }
                return Value.TrimStartAndEnd();
            }
        }
        return FString();
    }

    bool OpenWindowsDiscImageDialog(FString& SelectedDiscImagePath)
    {
        SelectedDiscImagePath.Reset();
#if PLATFORM_WINDOWS
        TCHAR FileName[MAX_PATH] = {};
        OPENFILENAME OpenFileName = {};
        OpenFileName.lStructSize = sizeof(OPENFILENAME);
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
        return false;
#endif
    }
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::VerifyPS1DiscImage(EPS1IsoGame Game, const FString& DiscImagePath)
{
    FPS1IsoVerificationResult Result;
    if (!IsSupportedGame(Game))
    {
        Result.Message = TEXT("An unsupported PS1 game was selected.");
        return Result;
    }
    if (DiscImagePath.TrimStartAndEnd().IsEmpty())
    {
        Result.Message = TEXT("No disc image path was supplied.");
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
    Result.bAllowedExtension = Extension == TEXT("iso") || Extension == TEXT("bin") || Extension == TEXT("cue");
    if (!Result.bAllowedExtension)
    {
        Result.Message = FString::Printf(TEXT("Disc image extension '.%s' is not allowed."), *Extension);
        return Result;
    }

    FDiscDataSource Source;
    Source.Path = FullDiscImagePath;
    if (Extension == TEXT("cue") && !ResolveCueDataFile(FullDiscImagePath, Source))
    {
        Result.Message = TEXT("CUE file did not describe a supported binary data track with INDEX 01.");
        return Result;
    }
    TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*Source.Path));
    if (!FileHandle)
    {
        Result.Message = FString::Printf(TEXT("Could not read disc image data: %s"), *Source.Path);
        return Result;
    }

    FDiscDirectoryReader Reader(*FileHandle, Source);
    TMap<FString, FDiscEntry> Entries;
    if (!Reader.ReadRootDirectory(Source.SectorSize, Entries, Result.Message))
    {
        return Result;
    }

    // Test only this game's executable alternatives before checking any other required files.
    TArray<const FDiscVariant*> MatchingVariants;
    TArray<FString> ExpectedExecutables;
    for (const FDiscVariant& Variant : DiscVariants)
    {
        if (Variant.Game != Game)
        {
            continue;
        }
        ExpectedExecutables.Add(Variant.BootExecutable);
        const FDiscEntry* Executable = Entries.Find(Variant.BootExecutable);
        if (Executable && !Executable->bDirectory && Reader.IsExtentInImage(Executable->Block, Executable->Size))
        {
            MatchingVariants.Add(&Variant);
        }
    }
    if (MatchingVariants.Num() == 0)
    {
        Result.MissingFiles.Add(FString::Join(ExpectedExecutables, TEXT(" OR ")));
        Result.Message = FString::Printf(TEXT("Disc image does not contain a supported %s executable (%s)."),
            *GetGameName(Game), *Result.MissingFiles[0]);
        return Result;
    }

    const FDiscEntry* SystemCnf = Entries.Find(TEXT("SYSTEM.CNF"));
    FString SystemCnfText;
    if (!SystemCnf || !Reader.ReadSystemCnf(*SystemCnf, SystemCnfText))
    {
        Result.MissingFiles.Add(TEXT("SYSTEM.CNF"));
        Result.Message = TEXT("Disc image is missing a readable SYSTEM.CNF file.");
        return Result;
    }
    Result.BootExecutable = GetBootExecutable(SystemCnfText);
    const FDiscVariant* SelectedVariant = nullptr;
    for (const FDiscVariant* Variant : MatchingVariants)
    {
        if (Result.BootExecutable == Variant->BootExecutable)
        {
            SelectedVariant = Variant;
            break;
        }
    }
    if (!SelectedVariant)
    {
        Result.Message = FString::Printf(TEXT("SYSTEM.CNF does not boot a supported %s executable."), *GetGameName(Game));
        return Result;
    }

    for (const FRequiredDiscEntry& Required : SelectedVariant->RequiredEntries)
    {
        const FDiscEntry* Entry = Entries.Find(Required.Name);
        if (!Entry || Entry->bDirectory != Required.bDirectory || !Reader.IsExtentInImage(Entry->Block, Entry->Size))
        {
            Result.MissingFiles.Add(Required.Name);
        }
    }
    if (Result.MissingFiles.Num() > 0)
    {
        Result.Message = FString::Printf(TEXT("Disc image is missing %d required file(s) or folder(s): %s."),
            Result.MissingFiles.Num(), *FString::Join(Result.MissingFiles, TEXT(", ")));
        return Result;
    }

    Result.bCanPlay = true;
    Result.Message = FString::Printf(TEXT("%s disc image verified. Access is enabled."), *GetGameName(Game));
    return Result;
}

bool UPS1IsoGateLibrary::ChoosePS1DiscImage(FString& SelectedDiscImagePath)
{
    return OpenWindowsDiscImageDialog(SelectedDiscImagePath);
}

FPS1IsoVerificationResult UPS1IsoGateLibrary::ChooseAndVerifyConfiguredPS1DiscImage(EPS1IsoGame Game, FString& SelectedDiscImagePath)
{
    FPS1IsoVerificationResult Result;
    if (!IsSupportedGame(Game))
    {
        SelectedDiscImagePath.Reset();
        Result.Message = TEXT("An unsupported PS1 game was selected.");
        return Result;
    }
    if (!ChoosePS1DiscImage(SelectedDiscImagePath))
    {
        Result.Message = TEXT("No PS1 disc image was selected.");
        return Result;
    }
    return VerifyPS1DiscImage(Game, SelectedDiscImagePath);
}
