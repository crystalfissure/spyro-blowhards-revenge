#include "WFKickableBoard.h"

namespace
{
constexpr float BoardAngleUnitToDegrees = 360.0f / 65536.0f;
}

AWFKickableBoard::AWFKickableBoard()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
    RootComponent = BoardMesh;
    BoardMesh->SetMobility(EComponentMobility::Movable);
    BoardMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ActMask = 0x3E;
}

void AWFKickableBoard::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (UprightMesh)
    {
        BoardMesh->SetStaticMesh(UprightMesh);
    }
}

void AWFKickableBoard::BeginPlay()
{
    Super::BeginPlay();
    HomeRotation = GetActorRotation();
}

void AWFKickableBoard::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActEnabled || SimulationHz <= KINDA_SMALL_NUMBER)
    {
        return;
    }
    const double StepSeconds = 1.0 / static_cast<double>(SimulationHz);
    StepAccumulator += FMath::Max(0.0f, DeltaSeconds);
    int32 CatchUp = 0;
    while (StepAccumulator + SMALL_NUMBER >= StepSeconds && CatchUp++ < 240)
    {
        StepAccumulator -= StepSeconds;
        StepBoard();
    }
}

bool AWFKickableBoard::HandleSM64Attack_Implementation(
    ESM64AttackType AttackType,
    AActor* InstigatorActor,
    FVector ImpactPoint,
    FVector ImpactDirection)
{
    if (BoardState == 0 && AttackType == ESM64AttackType::Charge)
    {
        BoardState = 1;
        StateTimer = 0;
        RockPhaseUnits = 0.0f;
        RockAmplitudeUnits = 1600.0f;
        LastInstigator = InstigatorActor;
        return true;
    }
    if (BoardState == 1 && StateTimer > 30 && AttackType == ESM64AttackType::Headbash
        && ImpactPoint.Z > GetActorLocation().Z + 160.0f)
    {
        BoardState = 2;
        StateTimer = 0;
        PitchVelocityUnits = 0.0f;
        LastInstigator = InstigatorActor;
        if (FelledMesh)
        {
            BoardMesh->SetStaticMesh(FelledMesh);
        }
        return true;
    }
    if (BoardState == 1 && AttackType == ESM64AttackType::Charge)
    {
        StateTimer = 0;
        RockPhaseUnits = 0.0f;
        RockAmplitudeUnits = 1600.0f;
        return true;
    }
    return false;
}

void AWFKickableBoard::StepBoard()
{
    FRotator Rotation = HomeRotation;
    if (BoardState == 0)
    {
        SetActorRotation(Rotation);
    }
    else if (BoardState == 1)
    {
        const float PhaseRadians = RockPhaseUnits * (2.0f * PI / 65536.0f);
        Rotation.Pitch += -FMath::Sin(PhaseRadians) * RockAmplitudeUnits * BoardAngleUnitToDegrees;
        SetActorRotation(Rotation);
        if (StateTimer != 0)
        {
            RockAmplitudeUnits -= 8.0f;
            if (RockAmplitudeUnits < 0.0f)
            {
                BoardState = 0;
                StateTimer = 0;
            }
        }
        else
        {
            RockAmplitudeUnits = 1600.0f;
            RockPhaseUnits = 0.0f;
        }
        RockPhaseUnits += 0x400;
    }
    else if (BoardState == 2)
    {
        PitchVelocityUnits -= 0x80;
        Rotation = GetActorRotation();
        Rotation.Pitch += PitchVelocityUnits * BoardAngleUnitToDegrees;
        if (Rotation.Pitch <= HomeRotation.Pitch - 90.0f)
        {
            Rotation.Pitch = HomeRotation.Pitch - 90.0f;
            BoardState = 3;
            PitchVelocityUnits = 0.0f;
            OnBoardFelled(LastInstigator.Get());
        }
        SetActorRotation(Rotation);
    }
    ++StateTimer;
}
