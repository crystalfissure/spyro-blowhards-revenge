#include "SM64CourseManager.h"

#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SM64ActActor.h"
#include "SM64PowerStar.h"
#include "SM64SessionSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
    USaveGame* FindLoadedSpyro64Save()
    {
        for (TObjectIterator<USaveGame> It; It; ++It)
        {
            USaveGame* Save = *It;
            if (!Save || Save->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
                || Save->IsPendingKill())
            {
                continue;
            }
            const FString ClassPath = Save->GetClass()->GetPathName();
            if (ClassPath.Contains(TEXT("/Game/Spyro64/64_SaveData_S1")))
            {
                return Save;
            }
        }
        return nullptr;
    }

    bool ReadNameIntMap(UObject* Owner, FName PropertyName, FName Key, int32& OutValue)
    {
        if (!Owner)
        {
            return false;
        }
        FMapProperty* MapProperty = FindFProperty<FMapProperty>(Owner->GetClass(), PropertyName);
        FNameProperty* KeyProperty = MapProperty ? CastField<FNameProperty>(MapProperty->KeyProp) : nullptr;
        FIntProperty* ValueProperty = MapProperty ? CastField<FIntProperty>(MapProperty->ValueProp) : nullptr;
        if (!MapProperty || !KeyProperty || !ValueProperty)
        {
            return false;
        }
        FScriptMapHelper Helper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Owner));
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index) && KeyProperty->GetPropertyValue(Helper.GetKeyPtr(Index)) == Key)
            {
                OutValue = ValueProperty->GetPropertyValue(Helper.GetValuePtr(Index));
                return true;
            }
        }
        return false;
    }

    bool WriteNameIntMap(UObject* Owner, FName PropertyName, FName Key, int32 Value)
    {
        if (!Owner)
        {
            return false;
        }
        FMapProperty* MapProperty = FindFProperty<FMapProperty>(Owner->GetClass(), PropertyName);
        FNameProperty* KeyProperty = MapProperty ? CastField<FNameProperty>(MapProperty->KeyProp) : nullptr;
        FIntProperty* ValueProperty = MapProperty ? CastField<FIntProperty>(MapProperty->ValueProp) : nullptr;
        if (!MapProperty || !KeyProperty || !ValueProperty)
        {
            return false;
        }
        FScriptMapHelper Helper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Owner));
        int32 FoundIndex = INDEX_NONE;
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index) && KeyProperty->GetPropertyValue(Helper.GetKeyPtr(Index)) == Key)
            {
                FoundIndex = Index;
                break;
            }
        }
        if (FoundIndex == INDEX_NONE)
        {
            FoundIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
            KeyProperty->SetPropertyValue(Helper.GetKeyPtr(FoundIndex), Key);
            Helper.Rehash();
            for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
            {
                if (Helper.IsValidIndex(Index) && KeyProperty->GetPropertyValue(Helper.GetKeyPtr(Index)) == Key)
                {
                    FoundIndex = Index;
                    break;
                }
            }
        }
        if (FoundIndex == INDEX_NONE)
        {
            return false;
        }
        ValueProperty->SetPropertyValue(Helper.GetValuePtr(FoundIndex), Value);
        return true;
    }
}

ASM64CourseManager::ASM64CourseManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ASM64CourseManager::BeginPlay()
{
    Super::BeginPlay();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (USM64SessionSubsystem* Session = GameInstance->GetSubsystem<USM64SessionSubsystem>())
        {
            const FName CourseId = CourseDefinition ? CourseDefinition->CourseId : FName(TEXT("WF"));
            int32 SelectedAct = CurrentAct;
            if (Session->ConsumePendingCourseSelection(CourseId, SelectedAct))
            {
                CurrentAct = SelectedAct;
            }
        }
    }
    LoadProgress();
    SetAct(CurrentAct, true);
}

ASM64CourseManager* ASM64CourseManager::FindCourseManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject || !WorldContextObject->GetWorld())
    {
        return nullptr;
    }
    for (TActorIterator<ASM64CourseManager> It(WorldContextObject->GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void ASM64CourseManager::SetAct(int32 NewAct, bool bResetTransientState)
{
    CurrentAct = FMath::Clamp(NewAct, 1, 6);
    if (bResetTransientState)
    {
        ResetTransientState();
    }
    ApplyActGating(bResetTransientState);
    OnActChanged.Broadcast(CurrentAct);
}

void ASM64CourseManager::ApplyActGating(bool bResetActors)
{
    for (TActorIterator<ASM64ActActor> It(GetWorld()); It; ++It)
    {
        It->SetCurrentAct(CurrentAct);
        if (bResetActors)
        {
            It->ResetForAct();
        }
    }
}

void ASM64CourseManager::ResetTransientState()
{
    CoinCount = 0;
    RedCoinCount = 0;
    bRedCoinGoalTriggered = false;
    b100CoinGoalTriggered = false;
    OnCoinCountChanged.Broadcast(CoinCount);
    OnRedCoinCountChanged.Broadcast(RedCoinCount);
}

void ASM64CourseManager::AddCoin(int32 Value, bool bRedCoin)
{
    CoinCount = FMath::Max(0, CoinCount + FMath::Max(0, Value));
    OnCoinCountChanged.Broadcast(CoinCount);
    if (!b100CoinGoalTriggered && CoinCount >= 100 && !Progress.bCollected100CoinStar)
    {
        b100CoinGoalTriggered = true;
        On100CoinGoalReached.Broadcast();
        RevealGoalStar(6, true, false);
    }
    if (bRedCoin)
    {
        RedCoinCount = FMath::Clamp(RedCoinCount + 1, 0, 8);
        OnRedCoinCountChanged.Broadcast(RedCoinCount);
        if (!bRedCoinGoalTriggered && RedCoinCount >= 8)
        {
            bRedCoinGoalTriggered = true;
            OnRedCoinGoalReached.Broadcast();
            RevealGoalStar(3, false, true);
        }
    }
}

void ASM64CourseManager::CollectStar(int32 StarIndex, bool bIs100CoinStar)
{
    if (bIs100CoinStar)
    {
        Progress.bCollected100CoinStar = true;
    }
    else
    {
        Progress.SetMissionStar(StarIndex);
        Progress.bCourseComplete = (Progress.MissionStarMask & 0x3F) == 0x3F;
    }
    SaveProgress();
    OnRequestSpyroSaveWrite(Progress);
    OnStarCollected.Broadcast(StarIndex, bIs100CoinStar);
    if (!bIs100CoinStar)
    {
        OnRequestMissionExit(StarIndex);
        if (bAutoOpenReturnLevel && CourseDefinition && !CourseDefinition->ReturnLevel.IsNone())
        {
            UGameplayStatics::OpenLevel(this, CourseDefinition->ReturnLevel);
        }
    }
}

void ASM64CourseManager::AwardOneUp(AActor* Collector)
{
    ++SessionOneUpCount;
    OnOneUpCollected.Broadcast(SessionOneUpCount);
    OnRequestSpyroOneUp(Collector, SessionOneUpCount);
}

void ASM64CourseManager::RevealGoalStar(int32 StarIndex, bool bIs100CoinStar, bool bRedCoinStyle)
{
    for (TActorIterator<ASM64PowerStar> It(GetWorld()); It; ++It)
    {
        ASM64PowerStar* Star = *It;
        if (!Star || Star->StarIndex != StarIndex || Star->b100CoinStar != bIs100CoinStar)
        {
            continue;
        }
        FVector SpawnLocation = Star->GetActorLocation();
        if (bIs100CoinStar)
        {
            if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
            {
                SpawnLocation = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f);
            }
        }
        Star->BeginSpawnSequence(SpawnLocation, SpawnLocation, bRedCoinStyle);
        return;
    }
}

void ASM64CourseManager::SetCannonUnlocked(bool bUnlocked)
{
    if (Progress.bCannonUnlocked == bUnlocked)
    {
        return;
    }
    Progress.bCannonUnlocked = bUnlocked;
    SaveProgress();
    OnRequestSpyroSaveWrite(Progress);
    OnCannonUnlockChanged(bUnlocked);
}

void ASM64CourseManager::RetryCurrentAct()
{
    SetAct(CurrentAct, true);
    if (CourseDefinition)
    {
        if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            FVector SpawnLocation = CourseDefinition->CourseStart;
            if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
            {
                SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
            }
            Pawn->SetActorLocationAndRotation(
                SpawnLocation,
                CourseDefinition->CourseStartRotation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
        }
    }
}

void ASM64CourseManager::LoadProgress()
{
    bUsingSpyroSaveLineage = bUseSpyroSaveLineage && TryLoadProgressFromSpyroSave();
    if (bUsingSpyroSaveLineage)
    {
        return;
    }
    if (!bUseStandaloneSaveFallback)
    {
        return;
    }
    USM64ProgressSaveGame* Save = Cast<USM64ProgressSaveGame>(
        UGameplayStatics::LoadGameFromSlot(StandaloneSaveSlot, 0));
    if (!Save)
    {
        return;
    }
    const FName CourseId = CourseDefinition ? CourseDefinition->CourseId : FName(TEXT("WF"));
    if (const FSM64CourseProgress* SavedProgress = Save->CourseProgress.Find(CourseId))
    {
        Progress = *SavedProgress;
    }
}

void ASM64CourseManager::SaveProgress()
{
    bUsingSpyroSaveLineage = bUseSpyroSaveLineage && TryWriteProgressToSpyroSave();
    if (!bUseStandaloneSaveFallback)
    {
        return;
    }
    USM64ProgressSaveGame* Save = Cast<USM64ProgressSaveGame>(
        UGameplayStatics::LoadGameFromSlot(StandaloneSaveSlot, 0));
    if (!Save)
    {
        Save = Cast<USM64ProgressSaveGame>(
            UGameplayStatics::CreateSaveGameObject(USM64ProgressSaveGame::StaticClass()));
    }
    if (!Save)
    {
        return;
    }
    const FName CourseId = CourseDefinition ? CourseDefinition->CourseId : FName(TEXT("WF"));
    Save->CourseProgress.Add(CourseId, Progress);
    UGameplayStatics::SaveGameToSlot(Save, StandaloneSaveSlot, 0);
}

bool ASM64CourseManager::TryLoadProgressFromSpyroSave()
{
    USaveGame* Save = FindLoadedSpyro64Save();
    if (!Save)
    {
        return false;
    }
    const FString CourseToken = CourseDefinition ? CourseDefinition->CourseId.ToString() : TEXT("WF");
    const FName StarsKey(*FString::Printf(TEXT("SM64_%s_Stars"), *CourseToken));
    const FName FlagsKey(*FString::Printf(TEXT("SM64_%s_Flags"), *CourseToken));
    int32 Stars = 0;
    int32 Flags = 0;
    const bool bHasStars = ReadNameIntMap(Save, SpyroProgressMapProperty, StarsKey, Stars);
    const bool bHasFlags = ReadNameIntMap(Save, SpyroProgressMapProperty, FlagsKey, Flags);
    if (!bHasStars && !bHasFlags)
    {
        // A compatible loaded save still becomes the authoritative target;
        // first-time course data simply starts empty.
        FMapProperty* MapProperty = FindFProperty<FMapProperty>(Save->GetClass(), SpyroProgressMapProperty);
        if (!MapProperty || !CastField<FNameProperty>(MapProperty->KeyProp)
            || !CastField<FIntProperty>(MapProperty->ValueProp))
        {
            return false;
        }
    }
    Progress.MissionStarMask = Stars & 0x3F;
    Progress.bCollected100CoinStar = (Flags & 0x01) != 0;
    Progress.bCannonUnlocked = (Flags & 0x02) != 0;
    Progress.bCourseComplete = (Flags & 0x04) != 0 || (Progress.MissionStarMask & 0x3F) == 0x3F;
    return true;
}

bool ASM64CourseManager::TryWriteProgressToSpyroSave()
{
    USaveGame* Save = FindLoadedSpyro64Save();
    if (!Save)
    {
        return false;
    }
    const FString CourseToken = CourseDefinition ? CourseDefinition->CourseId.ToString() : TEXT("WF");
    const FName StarsKey(*FString::Printf(TEXT("SM64_%s_Stars"), *CourseToken));
    const FName FlagsKey(*FString::Printf(TEXT("SM64_%s_Flags"), *CourseToken));
    int32 Flags = 0;
    Flags |= Progress.bCollected100CoinStar ? 0x01 : 0;
    Flags |= Progress.bCannonUnlocked ? 0x02 : 0;
    Flags |= Progress.bCourseComplete ? 0x04 : 0;
    return WriteNameIntMap(Save, SpyroProgressMapProperty, StarsKey, Progress.MissionStarMask & 0x3F)
        && WriteNameIntMap(Save, SpyroProgressMapProperty, FlagsKey, Flags);
}
