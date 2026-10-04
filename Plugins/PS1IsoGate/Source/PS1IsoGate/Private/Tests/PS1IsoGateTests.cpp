#include "PS1IsoGateLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace PS1IsoGateTests
{
    constexpr int32 BlockSize = 2048;
    constexpr int32 RootBlock = 20;
    struct FDiscTestVariant
    {
        EPS1IsoGame Game;
        const TCHAR* Executable;
        TArray<FString> Files;
        TArray<FString> Folders;
    };

    // Independent fixtures for the eight supplied directory listings.
    const FDiscTestVariant Variants[] =
    {
        { EPS1IsoGame::Spyro1, TEXT("SCUS_942.28"),
            { TEXT("PETEXA0.STR"), TEXT("PETEXA1.STR"), TEXT("PETEXA2.STR"), TEXT("PETEXA3.STR"), TEXT("PETEXA4.STR"), TEXT("PETEXA5.STR") },
            { TEXT("S0"), TEXT("SOURCE") } },
        { EPS1IsoGame::Spyro1, TEXT("SCES_014.38"),
            { TEXT("MUSIC1.STR"), TEXT("MUSIC2.STR"), TEXT("MUSIC3.STR"), TEXT("MUSIC4.STR"), TEXT("MUSIC5.STR"), TEXT("MUSIC6.STR") },
            { TEXT("S0"), TEXT("SOURCE") } },
        { EPS1IsoGame::Spyro1, TEXT("SCPS_100.85"),
            { TEXT("MUSIC1.STR"), TEXT("MUSIC2.STR"), TEXT("MUSIC3.STR"), TEXT("MUSIC4.STR"), TEXT("MUSIC5.STR"), TEXT("MUSIC6.STR") },
            { TEXT("SOURCE") } },
        { EPS1IsoGame::Spyro2, TEXT("SCUS_944.25"), { TEXT("SPEECH.STR"), TEXT("SPYRO2.TRD") }, { TEXT("KART") } },
        { EPS1IsoGame::Spyro2, TEXT("SCES_021.04"), { TEXT("SPEECH.STR"), TEXT("SPYRO2.TRD") }, { TEXT("KART") } },
        { EPS1IsoGame::Spyro2, TEXT("SCPS_101.28"), { TEXT("SPEECH.STR"), TEXT("SPYRO2.TRD") }, {} },
        { EPS1IsoGame::Spyro3, TEXT("SCUS_944.67"), { TEXT("3MN_BLNK.DAT"), TEXT("SPEECH.STR") }, { TEXT("CRASHBSH") } },
        { EPS1IsoGame::Spyro3, TEXT("SCES_028.35"), { TEXT("SPEECH.STR"), TEXT("SPYRO3.TRD") }, { TEXT("CRASHBSH") } }
    };

    void WriteBothEndian32(uint8* Bytes, uint32 Value)
    {
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Bytes[Index] = static_cast<uint8>(Value >> (8 * Index));
            Bytes[4 + Index] = static_cast<uint8>(Value >> (8 * (3 - Index)));
        }
    }

    TArray<uint8> MakeRecord(const FString& Name, bool bDirectory, uint32 Block, uint32 Size)
    {
        const FTCHARToUTF8 Identifier(*Name);
        TArray<uint8> Record;
        Record.SetNumZeroed(33 + Identifier.Length() + (Identifier.Length() % 2 == 0 ? 1 : 0));
        Record[0] = static_cast<uint8>(Record.Num());
        WriteBothEndian32(Record.GetData() + 2, Block);
        WriteBothEndian32(Record.GetData() + 10, Size);
        Record[25] = bDirectory ? 2 : 0;
        Record[28] = 1;
        Record[31] = 1;
        Record[32] = static_cast<uint8>(Identifier.Length());
        FMemory::Memcpy(Record.GetData() + 33, Identifier.Get(), Identifier.Length());
        return Record;
    }

    TArray<uint8> MakeIso(const FDiscTestVariant& Variant, const FString& Omitted = FString(),
        const FString& BootOverride = FString(), bool bExtraRootSector = false)
    {
        TArray<uint8> Image;
        Image.SetNumZeroed(128 * BlockSize);
        uint8* Pvd = Image.GetData() + 16 * BlockSize;
        Pvd[0] = 1;
        FMemory::Memcpy(Pvd + 1, "CD001", 5);
        Pvd[6] = 1;
        WriteBothEndian32(Pvd + 80, 128);
        Pvd[129] = 8;
        Pvd[130] = 8;
        TArray<uint8> Root = MakeRecord(TEXT("."), true, RootBlock, 2 * BlockSize);
        Root[33] = 0;
        FMemory::Memcpy(Pvd + 156, Root.GetData(), Root.Num());
        uint8* Terminator = Image.GetData() + 17 * BlockSize;
        Terminator[0] = 255;
        FMemory::Memcpy(Terminator + 1, "CD001", 5);
        Terminator[6] = 1;
        FMemory::Memcpy(Image.GetData() + RootBlock * BlockSize, Root.GetData(), Root.Num());
        Root[33] = 1;
        FMemory::Memcpy(Image.GetData() + RootBlock * BlockSize + Root.Num(), Root.GetData(), Root.Num());
        int32 RootPosition = RootBlock * BlockSize + 2 * Root.Num();
        if (bExtraRootSector)
        {
            RootPosition = (RootBlock + 1) * BlockSize;
        }
        auto AddEntry = [&Image, &RootPosition](const FString& Name, bool bDirectory, uint32 Block, uint32 Size)
        {
            const TArray<uint8> Record = MakeRecord(Name, bDirectory, Block, Size);
            if (RootPosition % BlockSize + Record.Num() > BlockSize)
            {
                RootPosition = (RootPosition / BlockSize + 1) * BlockSize;
            }
            FMemory::Memcpy(Image.GetData() + RootPosition, Record.GetData(), Record.Num());
            RootPosition += Record.Num();
        };
        TArray<FString> Files = { Variant.Executable, TEXT("SYSTEM.CNF"), TEXT("WAD.WAD") };
        Files.Append(Variant.Files);
        uint32 DataBlock = 40;
        for (const FString& Name : Files)
        {
            if (Name != Omitted)
            {
                const FString Content = Name == TEXT("SYSTEM.CNF")
                    ? FString::Printf(TEXT("BOOT = cdrom:\\%s;1\r\nTCB = 4\r\n"), BootOverride.IsEmpty() ? Variant.Executable : *BootOverride)
                    : TEXT("Synthetic test data");
                const FTCHARToUTF8 Bytes(*Content);
                AddEntry(Name + TEXT(";1"), false, DataBlock, Bytes.Length());
                FMemory::Memcpy(Image.GetData() + DataBlock * BlockSize, Bytes.Get(), Bytes.Length());
            }
            DataBlock += 2;
        }
        for (const FString& Folder : Variant.Folders)
        {
            if (Folder != Omitted)
            {
                AddEntry(Folder, true, DataBlock, BlockSize);
                DataBlock += 2;
            }
        }
        return Image;
    }

    TArray<uint8> ToRaw(const TArray<uint8>& Iso, int32 SectorSize, int32 DataOffset, int32 Pregap = 0)
    {
        TArray<uint8> Raw;
        Raw.SetNumZeroed((Iso.Num() / BlockSize + Pregap) * SectorSize);
        for (int32 Block = 0; Block < Iso.Num() / BlockSize; ++Block)
        {
            FMemory::Memcpy(Raw.GetData() + (Block + Pregap) * SectorSize + DataOffset, Iso.GetData() + Block * BlockSize, BlockSize);
        }
        return Raw;
    }

    class FFixtureFiles
    {
    public:
        FFixtureFiles()
        {
            Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PS1IsoGateTests") / FGuid::NewGuid().ToString());
            IFileManager::Get().MakeDirectory(*Directory, true);
        }
        ~FFixtureFiles() { IFileManager::Get().DeleteDirectory(*Directory, false, true); }
        FString Save(const FString& Name, const TArray<uint8>& Bytes)
        {
            const FString Path = Directory / Name;
            return FFileHelper::SaveArrayToFile(Bytes, *Path) ? Path : FString();
        }
        FString SaveText(const FString& Name, const FString& Text)
        {
            const FString Path = Directory / Name;
            return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
        }
    private:
        FString Directory;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS1IsoGateRegionsTest, "PS1IsoGate.Regions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPS1IsoGateRegionsTest::RunTest(const FString& Parameters)
{
    using namespace PS1IsoGateTests;
    FFixtureFiles Fixtures;
    for (const FDiscTestVariant& Variant : Variants)
    {
        const FString Path = Fixtures.Save(FString(Variant.Executable) + TEXT(".iso"), MakeIso(Variant));
        const FPS1IsoVerificationResult Result = UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Path);
        TestTrue(FString::Printf(TEXT("%s verifies: %s"), Variant.Executable, *Result.Message), Result.bCanPlay);
        TestEqual(TEXT("Reports regional executable"), Result.BootExecutable, FString(Variant.Executable));
        TestEqual(TEXT("No missing entries"), Result.MissingFiles.Num(), 0);
        for (EPS1IsoGame Other : { EPS1IsoGame::Spyro1, EPS1IsoGame::Spyro2, EPS1IsoGame::Spyro3 })
        {
            if (Other != Variant.Game)
            {
                const FPS1IsoVerificationResult Wrong = UPS1IsoGateLibrary::VerifyPS1DiscImage(Other, Path);
                TestFalse(TEXT("Rejects another selected game"), Wrong.bCanPlay);
                TestTrue(TEXT("Executable checked before other files"), Wrong.Message.Contains(TEXT("executable")));
                TestTrue(TEXT("SYSTEM.CNF not read for wrong game"), Wrong.BootExecutable.IsEmpty());
            }
        }
        TArray<FString> Required = { Variant.Executable, TEXT("SYSTEM.CNF"), TEXT("WAD.WAD") };
        Required.Append(Variant.Files);
        Required.Append(Variant.Folders);
        for (const FString& Missing : Required)
        {
            const FString MissingPath = Fixtures.Save(TEXT("missing.iso"), MakeIso(Variant, Missing));
            const FPS1IsoVerificationResult MissingResult = UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, MissingPath);
            TestFalse(FString::Printf(TEXT("%s requires %s"), Variant.Executable, *Missing), MissingResult.bCanPlay);
            TestTrue(TEXT("Reports missing entry"), MissingResult.MissingFiles.ContainsByPredicate(
                [&Missing](const FString& Name) { return Name.Contains(Missing); }));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS1IsoGateFormatsTest, "PS1IsoGate.Formats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPS1IsoGateFormatsTest::RunTest(const FString& Parameters)
{
    using namespace PS1IsoGateTests;
    FFixtureFiles Fixtures;
    struct FFormat { int32 Size; int32 Offset; const TCHAR* Mode; };
    const FFormat Formats[] = { { 2048, 0, TEXT("MODE1/2048") }, { 2352, 16, TEXT("MODE1/2352") },
        { 2352, 24, TEXT("MODE2/2352") }, { 2336, 8, TEXT("MODE2/2336") } };
    for (const FDiscTestVariant& Variant : Variants)
    {
        const TArray<uint8> Iso = MakeIso(Variant, FString(), FString(), true);
        for (const FFormat& Format : Formats)
        {
            const FString Bin = Fixtures.Save(TEXT("disc data.BIN"), ToRaw(Iso, Format.Size, Format.Offset));
            TestTrue(TEXT("Regional disc verifies in each layout"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Bin).bCanPlay);
            const FString Cue = Fixtures.SaveText(TEXT("disc.cue"), FString::Printf(
                TEXT("FILE \"disc data.BIN\" BINARY\n TRACK 01 %s\n INDEX 01 00:00:00\n"), Format.Mode));
            TestTrue(TEXT("CUE with quoted filename"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Cue).bCanPlay);
            const FString Pregap = Fixtures.Save(TEXT("disc data.BIN"), ToRaw(Iso, Format.Size, Format.Offset, 150));
            TestFalse(TEXT("Pregap fixture saved"), Pregap.IsEmpty());
            const FString PregapCue = Fixtures.SaveText(TEXT("pregap.cue"), FString::Printf(
                TEXT("FILE \"audio.bin\" BINARY\n TRACK 01 AUDIO\n INDEX 01 00:00:00\n")
                TEXT("FILE \"disc data.BIN\" BINARY\n TRACK 02 %s\n INDEX 00 00:00:00\n INDEX 01 00:02:00\n"), Format.Mode));
            TestTrue(TEXT("CUE uses data track and stored pregap"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, PregapCue).bCanPlay);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS1IsoGateFailuresTest, "PS1IsoGate.Failures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPS1IsoGateFailuresTest::RunTest(const FString& Parameters)
{
    using namespace PS1IsoGateTests;
    FFixtureFiles Fixtures;
    const FDiscTestVariant& Variant = Variants[0];
    TestFalse(TEXT("Empty path"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, TEXT("  ")).bCanPlay);
    TestFalse(TEXT("Missing file"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, TEXT("missing-disc.iso")).bExists);
    const FString Valid = Fixtures.Save(TEXT("valid.iso"), MakeIso(Variant));
    TestFalse(TEXT("Invalid enum"), UPS1IsoGateLibrary::VerifyPS1DiscImage(static_cast<EPS1IsoGame>(255), Valid).bCanPlay);
    const FString TextFile = Fixtures.Save(TEXT("disc.txt"), MakeIso(Variant));
    TestFalse(TEXT("Wrong extension"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, TextFile).bAllowedExtension);
    const FString Mismatch = Fixtures.Save(TEXT("mismatch.iso"), MakeIso(Variant, FString(), TEXT("SCUS_944.25")));
    TestFalse(TEXT("Boot target must match executable"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Mismatch).bCanPlay);
    TArray<uint8> Image = MakeIso(Variant, TEXT("WAD.WAD"));
    FMemory::Memcpy(Image.GetData(), "WAD.WAD", 7);
    const FString Marker = Fixtures.Save(TEXT("marker.iso"), Image);
    TestFalse(TEXT("Embedded bytes cannot replace file entry"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Marker).bCanPlay);
    Image = MakeIso(Variant);
    Image[RootBlock * BlockSize] = 1;
    const FString Malformed = Fixtures.Save(TEXT("malformed.iso"), Image);
    TestFalse(TEXT("Malformed directory"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Malformed).bCanPlay);
    Image = MakeIso(Variant);
    WriteBothEndian32(Image.GetData() + 16 * BlockSize + 166, MAX_uint32);
    const FString Oversized = Fixtures.Save(TEXT("oversized.iso"), Image);
    TestFalse(TEXT("Root allocation bounded"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Oversized).bCanPlay);
    Image = MakeIso(Variant);
    Image.SetNum(41 * BlockSize);
    const FString Truncated = Fixtures.Save(TEXT("truncated.iso"), Image);
    TestFalse(TEXT("Rejects truncated extents"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Truncated).bCanPlay);
    const FString MissingCue = Fixtures.SaveText(TEXT("missing.cue"), TEXT("FILE \"absent.bin\" BINARY\n TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n"));
    TestFalse(TEXT("Missing CUE target"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, MissingCue).bCanPlay);
    const FString NoIndex = Fixtures.SaveText(TEXT("no-index.cue"), TEXT("FILE \"valid.iso\" BINARY\n TRACK 01 MODE1/2048\n"));
    TestFalse(TEXT("CUE needs INDEX 01"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, NoIndex).bCanPlay);
    const FString BadTime = Fixtures.SaveText(TEXT("time.cue"), TEXT("FILE \"valid.iso\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:60:00\n"));
    TestFalse(TEXT("Invalid CUE time"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, BadTime).bCanPlay);
    const FString Audio = Fixtures.SaveText(TEXT("audio.cue"), TEXT("FILE \"valid.iso\" BINARY\n TRACK 01 AUDIO\n INDEX 01 00:00:00\n"));
    TestFalse(TEXT("Audio-only CUE"), UPS1IsoGateLibrary::VerifyPS1DiscImage(Variant.Game, Audio).bCanPlay);
    return true;
}
#endif
