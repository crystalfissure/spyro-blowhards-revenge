#include "WFTowerPlatformGroup.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AWFTowerPlatformGroup::AWFTowerPlatformGroup()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    PlatformClass = ASM64MovingPlatformBase::StaticClass();
    ActMask = 0x3E;
}

void AWFTowerPlatformGroup::SetCurrentAct(int32 NewAct)
{
    Super::SetCurrentAct(NewAct);
    if (!bActEnabled)
    {
        DestroyPlatforms();
    }
}

void AWFTowerPlatformGroup::ResetForAct_Implementation()
{
    DestroyPlatforms();
}

void AWFTowerPlatformGroup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActEnabled)
    {
        return;
    }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn)
    {
        return;
    }
    const float Threshold = GetActorLocation().Z - ActivationHeightBelowRoot;
    if (!bSpawned && Pawn->GetActorLocation().Z > Threshold)
    {
        SpawnPlatforms();
    }
    else if (bSpawned && Pawn->GetActorLocation().Z < Threshold)
    {
        DestroyPlatforms();
    }
}

void AWFTowerPlatformGroup::SpawnPlatforms()
{
    if (bSpawned || !PlatformClass)
    {
        return;
    }
    bSpawned = true;
    Platforms.Reset();
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float SourceYaw = Index * 45.0f;
        const float Radians = FMath::DegreesToRadians(SourceYaw);
        FVector Location = GetActorLocation();
        Location.X += Radius * FMath::Sin(Radians);
        Location.Y += Radius * FMath::Cos(Radians);
        Location.Z += 300.0f + PlatformHeightStep * Index;
        const FRotator Rotation(0.0f, -SourceYaw, 0.0f);
        const FTransform SpawnTransform(Rotation, Location);
        ASM64MovingPlatformBase* Platform = GetWorld()->SpawnActorDeferred<ASM64MovingPlatformBase>(
            PlatformClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Platform)
        {
            continue;
        }
        Platform->StableId = FName(*FString::Printf(TEXT("WF_TowerPlatform_%02d"), Index));
        Platform->DefaultMesh = Index == 7 && ElevatorMesh ? ElevatorMesh : PlatformMesh;
        Platform->DefaultCollisionMesh = PlatformCollisionMesh;
        Platform->InitialPhaseFrames = 0;
        if (Index == 7)
        {
            Platform->Motion = ESM64PlatformMotion::TowerElevator;
        }
        else if (Index == 1 || Index == 3 || Index == 5)
        {
            Platform->Motion = ESM64PlatformMotion::TowerSliding;
            Platform->TravelDistance = 380.0f;
            Platform->SpeedPerFrame = 3.0f;
            // SM64 forward velocity is (sin(yaw), cos(yaw)) in source X/Z.
            // After source Z maps to UE Y this remains (sin, cos), while the
            // visible mesh yaw itself is negated independently.
            Platform->MotionDirection = FVector(FMath::Sin(Radians), FMath::Cos(Radians), 0.0f);
        }
        UGameplayStatics::FinishSpawningActor(Platform, SpawnTransform);
        Platforms.Add(Platform);
    }
}

void AWFTowerPlatformGroup::DestroyPlatforms()
{
    for (TWeakObjectPtr<ASM64MovingPlatformBase>& Platform : Platforms)
    {
        if (Platform.IsValid())
        {
            Platform->Destroy();
        }
    }
    Platforms.Reset();
    bSpawned = false;
}
