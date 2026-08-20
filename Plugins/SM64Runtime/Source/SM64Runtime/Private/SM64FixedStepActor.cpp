#include "SM64FixedStepActor.h"

#include "SM64CourseManager.h"

ASM64FixedStepActor::ASM64FixedStepActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASM64FixedStepActor::BeginPlay()
{
    Super::BeginPlay();
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        RandomSeed = Manager->SessionSeed;
        SetCurrentAct(Manager->CurrentAct);
    }
    ResetForAct();
}

void ASM64FixedStepActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bActEnabled || SimulationRate <= 0.0f)
    {
        return;
    }

    const double StepSeconds = 1.0 / static_cast<double>(SimulationRate);
    FixedStepAccumulator += FMath::Max(0.0f, DeltaSeconds);

    int32 Steps = 0;
    while (FixedStepAccumulator + SMALL_NUMBER >= StepSeconds && Steps < MaxCatchUpStepsPerTick)
    {
        SimulateSM64Frame();
        OnSimulationFrame(SimulationFrame);
        ++SimulationFrame;
        FixedStepAccumulator -= StepSeconds;
        ++Steps;
    }
}

void ASM64FixedStepActor::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    FixedStepAccumulator = 0.0;
    SimulationFrame = 0;
    ActionCode = 0;
    ActionTimer = 0;
    DeterministicRandom.Initialize(RandomSeed ^ static_cast<int32>(GetTypeHash(StableId)));
}

void ASM64FixedStepActor::SimulateSM64Frame_Implementation()
{
}

void ASM64FixedStepActor::SetActionCode(int32 NewAction)
{
    if (ActionCode == NewAction)
    {
        return;
    }

    const int32 PreviousAction = ActionCode;
    ActionCode = NewAction;
    ActionTimer = 0;
    OnActionChanged(PreviousAction, NewAction);
}

void ASM64FixedStepActor::FinishActionFrame(int32 PreviousAction)
{
    if (PreviousAction == ActionCode)
    {
        ++ActionTimer;
    }
    else
    {
        ActionTimer = 0;
    }
}

int32 ASM64FixedStepActor::RandomIntegerExclusive(int32 MaxExclusive)
{
    return MaxExclusive > 0 ? DeterministicRandom.RandRange(0, MaxExclusive - 1) : 0;
}

float ASM64FixedStepActor::RandomFloat()
{
    return DeterministicRandom.FRand();
}

FVector ASM64FixedStepActor::GetSM64ForwardVector() const
{
    const float StoredUEYawRadians = FMath::DegreesToRadians(GetActorRotation().Yaw);
    return FVector(-FMath::Sin(StoredUEYawRadians), FMath::Cos(StoredUEYawRadians), 0.0f);
}

float ASM64FixedStepActor::GetSM64YawTowardLocation(const FVector& TargetLocation) const
{
    const FVector Delta = TargetLocation - GetActorLocation();
    return -FMath::RadiansToDegrees(FMath::Atan2(Delta.X, Delta.Y));
}

void ASM64FixedStepActor::TurnSM64YawTowardLocation(const FVector& TargetLocation, float MaxDegreesPerFrame)
{
    FRotator Rotation = GetActorRotation();
    Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, GetSM64YawTowardLocation(TargetLocation), MaxDegreesPerFrame);
    SetActorRotation(Rotation);
}
