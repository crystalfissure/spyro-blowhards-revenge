#include "SM64StarMarker.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64CourseManager.h"

ASM64StarMarker::ASM64StarMarker()
{
    bPowerStar = true;
    bRespawnOnActReset = false;
}

void ASM64StarMarker::BeginPlay()
{
    Super::BeginPlay();
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        if (Goal == ESM64StarMarkerGoal::EightRedCoins)
        {
            Manager->OnRedCoinGoalReached.AddDynamic(this, &ASM64StarMarker::OnRedCoinGoal);
        }
        else if (Goal == ESM64StarMarkerGoal::HundredCoins)
        {
            Manager->On100CoinGoalReached.AddDynamic(this, &ASM64StarMarker::OnHundredCoinGoal);
        }
    }
}

bool ASM64StarMarker::IsAlreadyCollected() const
{
    const ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this);
    if (!Manager)
    {
        return false;
    }
    return Goal == ESM64StarMarkerGoal::HundredCoins
        ? Manager->Progress.bCollected100CoinStar
        : Manager->Progress.HasMissionStar(StarIndex);
}

void ASM64StarMarker::ResetForAct_Implementation()
{
    b100CoinStar = Goal == ESM64StarMarkerGoal::HundredCoins;
    bCollected = false;
    bRevealed = false;
    if (IsAlreadyCollected() || !bActEnabled)
    {
        SetActorHiddenInGame(true);
        Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    else if (bInitiallyRevealed)
    {
        RevealStar();
    }
    else
    {
        SetActorHiddenInGame(true);
        Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ASM64StarMarker::RevealStar()
{
    if (bRevealed || IsAlreadyCollected() || !bActEnabled)
    {
        return;
    }
    if (bRevealAtPlayerLocation)
    {
        if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            SetActorLocation(Player->GetActorLocation() + RevealLocationOffset);
        }
    }
    bCollected = false;
    bRevealed = true;
    SetActorHiddenInGame(false);
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OnStarMarkerRevealed(GetActorLocation(), b100CoinStar);
}

void ASM64StarMarker::OnRedCoinGoal()
{
    if (Goal == ESM64StarMarkerGoal::EightRedCoins)
    {
        RevealStar();
    }
}

void ASM64StarMarker::OnHundredCoinGoal()
{
    if (Goal == ESM64StarMarkerGoal::HundredCoins)
    {
        RevealStar();
    }
}
