#include "WFCheckerboardElevatorPair.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64CourseManager.h"

AWFCheckerboardElevatorPair::AWFCheckerboardElevatorPair()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    PlatformRootA = CreateDefaultSubobject<USceneComponent>(TEXT("PlatformRootA"));
    PlatformRootA->SetupAttachment(SceneRoot);
    PlatformRootB = CreateDefaultSubobject<USceneComponent>(TEXT("PlatformRootB"));
    PlatformRootB->SetupAttachment(SceneRoot);

    RenderMeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RenderMeshA"));
    RenderMeshA->SetupAttachment(PlatformRootA);
    RenderMeshA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RenderMeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RenderMeshB"));
    RenderMeshB->SetupAttachment(PlatformRootB);
    RenderMeshB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CollisionMeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionMeshA"));
    CollisionMeshA->SetupAttachment(PlatformRootA);
    CollisionMeshA->SetVisibility(false, true);
    CollisionMeshA->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionMeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionMeshB"));
    CollisionMeshB->SetupAttachment(PlatformRootB);
    CollisionMeshB->SetVisibility(false, true);
    CollisionMeshB->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AWFCheckerboardElevatorPair::ConfigureVariant()
{
    EffectiveMoveDuration = MoveDurationParameter == 0 ? 65 : MoveDurationParameter;
    const bool bLargeVariant = GroupVariant == 1;
    const float LateralOffset = bLargeVariant ? 235.0f : 145.0f;
    RotationRadius = bLargeVariant ? 11.6f : 7.0f;
    // Source gfx scales (x,y,z) become UE (x,z,y).
    VariantScale = bLargeVariant ? FVector(1.2f, 1.2f, 2.0f) : FVector(0.7f, 0.7f, 1.5f);
    InitialRelativeLocations[0] = FVector(0.0f, -LateralOffset, 0.0f);
    InitialRelativeLocations[1] = FVector(0.0f, LateralOffset, EffectiveMoveDuration * 10.0f);
}

void AWFCheckerboardElevatorPair::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureVariant();
    PlatformRootA->SetRelativeLocation(InitialRelativeLocations[0]);
    PlatformRootB->SetRelativeLocation(InitialRelativeLocations[1]);
    RenderMeshA->SetRelativeScale3D(VariantScale);
    RenderMeshB->SetRelativeScale3D(VariantScale);
    CollisionMeshA->SetRelativeScale3D(VariantScale);
    CollisionMeshB->SetRelativeScale3D(VariantScale);
    RenderMeshA->SetStaticMesh(DefaultRenderMesh);
    RenderMeshB->SetStaticMesh(DefaultRenderMesh);
    CollisionMeshA->SetStaticMesh(DefaultCollisionMesh);
    CollisionMeshB->SetStaticMesh(DefaultCollisionMesh);
    const ECollisionEnabled::Type CollisionMode = DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
    CollisionMeshA->SetCollisionEnabled(CollisionMode);
    CollisionMeshB->SetCollisionEnabled(CollisionMode);
}

void AWFCheckerboardElevatorPair::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    ConfigureVariant();
    PlatformRootA->SetRelativeLocationAndRotation(InitialRelativeLocations[0], FRotator::ZeroRotator);
    PlatformRootB->SetRelativeLocationAndRotation(InitialRelativeLocations[1], FRotator::ZeroRotator);
    PlatformActions[0] = 0;
    PlatformActions[1] = 0;
    PlatformTimers[0] = PlatformTimers[1] = 0;
    PlatformPitchDegrees[0] = PlatformPitchDegrees[1] = 0.0f;
}

int32 AWFCheckerboardElevatorPair::GetPlatformAction(int32 PlatformIndex) const
{
    return PlatformIndex >= 0 && PlatformIndex < 2 ? PlatformActions[PlatformIndex] : INDEX_NONE;
}

void AWFCheckerboardElevatorPair::SetPlatformAction(int32 Index, int32 NewAction)
{
    if (PlatformActions[Index] != NewAction)
    {
        PlatformActions[Index] = NewAction;
        PlatformTimers[Index] = 0;
    }
}

void AWFCheckerboardElevatorPair::StepPlatform(int32 Index, USceneComponent* PlatformRoot)
{
    const int32 PreviousAction = PlatformActions[Index];
    const FVector PreviousLocation = PlatformRoot->GetComponentLocation();
    const FRotator PreviousRotation = PlatformRoot->GetComponentRotation();
    FVector FrameDelta = FVector::ZeroVector;

    switch (PlatformActions[Index])
    {
        case 0:
            SetPlatformAction(Index, Index == 0 ? 1 : 3);
            break;

        case 1:
            FrameDelta.Z = 10.0f;
            PlatformPitchDegrees[Index] = 0.0f;
            if (PlatformTimers[Index] > EffectiveMoveDuration)
            {
                SetPlatformAction(Index, 2);
            }
            break;

        case 2:
        {
            PlatformPitchDegrees[Index] += 2.8125f; // abs(512) angle units
            const float PitchRadians = FMath::DegreesToRadians(PlatformPitchDegrees[Index]);
            FrameDelta = GetSM64ForwardVector() * (FMath::Sin(PitchRadians) * RotationRadius);
            FrameDelta.Z = FMath::Cos(PitchRadians) * RotationRadius;
            if (PlatformTimers[Index] + 1 == 64)
            {
                SetPlatformAction(Index, 3);
            }
            break;
        }

        case 3:
            FrameDelta.Z = -10.0f;
            PlatformPitchDegrees[Index] = 0.0f;
            if (PlatformTimers[Index] > EffectiveMoveDuration)
            {
                SetPlatformAction(Index, 4);
            }
            break;

        case 4:
        {
            PlatformPitchDegrees[Index] += 2.8125f;
            const float PitchRadians = FMath::DegreesToRadians(PlatformPitchDegrees[Index]);
            FrameDelta = GetSM64ForwardVector() * (-FMath::Sin(PitchRadians) * RotationRadius);
            FrameDelta.Z = -FMath::Cos(PitchRadians) * RotationRadius;
            if (PlatformTimers[Index] + 1 == 64)
            {
                SetPlatformAction(Index, 1);
                PlatformPitchDegrees[Index] = 0.0f;
                if (Index == 1)
                {
                    OnPairCycleCompleted.Broadcast();
                }
            }
            break;
        }
    }

    PlatformRoot->AddWorldOffset(FrameDelta, false);
    PlatformRoot->SetRelativeRotation(FRotator(PlatformPitchDegrees[Index], 0.0f, 0.0f));
    OnCheckerboardPlatformDelta(Index, PlatformRoot->GetComponentLocation() - PreviousLocation,
        PlatformRoot->GetComponentRotation() - PreviousRotation, PlatformActions[Index]);

    if (PreviousAction == PlatformActions[Index])
    {
        ++PlatformTimers[Index];
    }
    else
    {
        PlatformTimers[Index] = 0;
    }
}

void AWFCheckerboardElevatorPair::SimulateSM64Frame_Implementation()
{
    StepPlatform(0, PlatformRootA);
    StepPlatform(1, PlatformRootB);
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    OnCheckerboardElevatorSound(Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 1000.0f);
}
