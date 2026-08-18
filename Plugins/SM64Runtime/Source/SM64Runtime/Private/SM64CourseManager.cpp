#include "SM64CourseManager.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SM64ActActor.h"

ASM64CourseManager::ASM64CourseManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ASM64CourseManager::BeginPlay()
{
    Super::BeginPlay();
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
    ApplyActGating();
    OnActChanged.Broadcast(CurrentAct);
}

void ASM64CourseManager::ApplyActGating()
{
    for (TActorIterator<ASM64ActActor> It(GetWorld()); It; ++It)
    {
        It->SetCurrentAct(CurrentAct);
    }
}

void ASM64CourseManager::ResetTransientState()
{
    CoinCount = 0;
    RedCoinCount = 0;
    OnCoinCountChanged.Broadcast(CoinCount);
    OnRedCoinCountChanged.Broadcast(RedCoinCount);
}

void ASM64CourseManager::AddCoin(int32 Value, bool bRedCoin)
{
    CoinCount = FMath::Max(0, CoinCount + FMath::Max(0, Value));
    OnCoinCountChanged.Broadcast(CoinCount);
    if (bRedCoin)
    {
        RedCoinCount = FMath::Clamp(RedCoinCount + 1, 0, 8);
        OnRedCoinCountChanged.Broadcast(RedCoinCount);
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
    }
    SaveProgress();
    OnRequestSpyroSaveWrite(Progress);
    OnStarCollected.Broadcast(StarIndex, bIs100CoinStar);
    if (!bIs100CoinStar)
    {
        OnRequestMissionExit(StarIndex);
    }
}

void ASM64CourseManager::RetryCurrentAct()
{
    SetAct(CurrentAct, true);
    if (CourseDefinition)
    {
        if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            Pawn->SetActorLocationAndRotation(
                CourseDefinition->CourseStart,
                CourseDefinition->CourseStartRotation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
        }
    }
}

void ASM64CourseManager::LoadProgress()
{
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
