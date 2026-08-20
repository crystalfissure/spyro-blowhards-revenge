#include "SM64MovingPlatformBase.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/Crc.h"
#include "SM64CourseManager.h"

namespace
{
constexpr float PlatformAngleUnitToDegrees = 360.0f / 65536.0f;
}

ASM64MovingPlatformBase::ASM64MovingPlatformBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    SceneRoot->SetMobility(EComponentMobility::Movable);

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(SceneRoot);
    PlatformMesh->SetMobility(EComponentMobility::Movable);
    PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlatformMesh->SetGenerateOverlapEvents(false);

    CollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionMesh"));
    CollisionMesh->SetupAttachment(SceneRoot);
    CollisionMesh->SetMobility(EComponentMobility::Movable);
    CollisionMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionMesh->SetGenerateOverlapEvents(false);
    CollisionMesh->SetVisibility(false, true);
    CollisionMesh->SetHiddenInGame(true, true);

    RiderSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("RiderSensor"));
    RiderSensor->SetupAttachment(SceneRoot);
    RiderSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RiderSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
    RiderSensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    RiderSensor->SetGenerateOverlapEvents(true);
}

void ASM64MovingPlatformBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        PlatformMesh->SetStaticMesh(DefaultMesh);
    }
    if (DefaultCollisionMesh)
    {
        CollisionMesh->SetStaticMesh(DefaultCollisionMesh);
    }
    RiderSensor->SetBoxExtent(RiderSensorExtent);
    RiderSensor->SetRelativeLocation(RiderSensorOffset);
}

void ASM64MovingPlatformBase::BeginPlay()
{
    Super::BeginPlay();
    if (const ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        SessionSeed = Manager->SessionSeed;
    }
    RiderSensor->OnComponentBeginOverlap.AddDynamic(this, &ASM64MovingPlatformBase::OnRiderBeginOverlap);
    RiderSensor->OnComponentEndOverlap.AddDynamic(this, &ASM64MovingPlatformBase::OnRiderEndOverlap);
    HomeTransform = GetActorTransform();
    if (Motion == ESM64PlatformMotion::Sliding)
    {
        FVector Adjusted = HomeTransform.GetLocation();
        Adjusted.X += 2.0f;
        HomeTransform.SetLocation(Adjusted);
    }
    bHomeTransformInitialized = true;
    ResetMotion();
}

void ASM64MovingPlatformBase::SetCurrentAct(int32 NewAct)
{
    Super::SetCurrentAct(NewAct);
}

void ASM64MovingPlatformBase::ResetForAct_Implementation()
{
    ResetMotion();
}

void ASM64MovingPlatformBase::ResetMotion()
{
    if (!bHomeTransformInitialized)
    {
        HomeTransform = GetActorTransform();
        bHomeTransformInitialized = true;
    }
    SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);

    const FString Identity = StableId.IsNone() ? GetPathName() : StableId.ToString();
    RandomStream.Initialize(SessionSeed ^ static_cast<int32>(FCrc::StrCrc32(*Identity)));
    SimulationFrame = 0;
    StepAccumulator = 0.0;
    MotionAction = 0;
    ActionTimer = InitialPhaseFrames >= 0
        ? InitialPhaseFrames
        : RandomStream.RandRange(0, 99);
    VerticalVelocity = 0.0f;
    PitchVelocity = 0.0f;
    RollVelocity = 0.0f;
    RollAcceleration = 0.0f;
    TumblingFloorHeight = HomeTransform.GetLocation().Z - 10000.0f;
    CurrentForwardSpeed = 0.0f;
    Riders.Reset();
}

void ASM64MovingPlatformBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActEnabled || SimulationHz <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const double StepSeconds = 1.0 / static_cast<double>(SimulationHz);
    StepAccumulator += FMath::Max(0.0f, DeltaSeconds);
    int32 CatchUpSteps = 0;
    while (StepAccumulator + SMALL_NUMBER >= StepSeconds && CatchUpSteps < 240)
    {
        StepAccumulator -= StepSeconds;
        StepSimulation();
        OnSM64SimulationStep(SimulationFrame);
        ++SimulationFrame;
        ++CatchUpSteps;
    }
}

void ASM64MovingPlatformBase::SetMotionAction(int32 NewAction)
{
    MotionAction = NewAction;
    ActionTimer = bInsideSimulationStep ? -1 : 0;
}

void ASM64MovingPlatformBase::StepSimulation()
{
    bInsideSimulationStep = true;
    FTransform Next = GetActorTransform();
    FVector Location = Next.GetLocation();
    FRotator Rotation = Next.Rotator();
    const FVector Direction = MotionDirection.IsNearlyZero()
        ? FVector::ForwardVector
        : MotionDirection.GetSafeNormal();

    switch (Motion)
    {
    case ESM64PlatformMotion::Sliding:
        if (MotionAction == 0)
        {
            if (ActionTimer > 100)
            {
                CurrentForwardSpeed = SpeedPerFrame;
                SetMotionAction(1);
            }
        }
        else if (MotionAction == 1)
        {
            if (static_cast<float>(ActionTimer) >= 500.0f / FMath::Max(1.0f, SpeedPerFrame))
            {
                Location.X = HomeTransform.GetLocation().X + TravelDistance;
                CurrentForwardSpeed = 0.0f;
            }
            if (ActionTimer == 60)
            {
                CurrentForwardSpeed = -SpeedPerFrame;
                SetMotionAction(2);
            }
        }
        else
        {
            if (static_cast<float>(ActionTimer) >= 500.0f / FMath::Max(1.0f, SpeedPerFrame))
            {
                Location.X = HomeTransform.GetLocation().X;
                CurrentForwardSpeed = 0.0f;
            }
            if (ActionTimer == 90)
            {
                CurrentForwardSpeed = SpeedPerFrame;
                SetMotionAction(1);
            }
        }
        // OBJ_FLAG_MOVE_XZ_USING_FVEL is evaluated after the behavior script,
        // so a velocity/action change affects this same source frame.
        Location.X += CurrentForwardSpeed;
        break;

    case ESM64PlatformMotion::SmallBomp:
    case ESM64PlatformMotion::LargeBomp:
        if (MotionAction == 0)
        {
            if (ActionTimer > 100)
            {
                CurrentForwardSpeed = 30.0f;
                SetMotionAction(1);
            }
        }
        else if (MotionAction == 1)
        {
            if (Location.X > HomeTransform.GetLocation().X + 150.0f)
            {
                Location.X = HomeTransform.GetLocation().X + 150.0f;
                CurrentForwardSpeed = 0.0f;
            }
            if (ActionTimer == 15)
            {
                CurrentForwardSpeed = Motion == ESM64PlatformMotion::SmallBomp ? 40.0f : 10.0f;
                SetMotionAction(2);
            }
        }
        else if (MotionAction == 2)
        {
            if (Location.X > HomeTransform.GetLocation().X + 530.0f)
            {
                Location.X = HomeTransform.GetLocation().X + 530.0f;
                CurrentForwardSpeed = 0.0f;
            }
            if (ActionTimer == 60)
            {
                CurrentForwardSpeed = -10.0f;
                SetMotionAction(3);
            }
        }
        else
        {
            if (Location.X < HomeTransform.GetLocation().X + 30.0f)
            {
                Location.X = HomeTransform.GetLocation().X + 30.0f;
                CurrentForwardSpeed = 0.0f;
            }
            if (ActionTimer == 90)
            {
                CurrentForwardSpeed = 25.0f;
                SetMotionAction(1);
            }
        }
        Location.X += CurrentForwardSpeed;
        break;

    case ESM64PlatformMotion::RotatingWood:
        if (MotionAction == 0)
        {
            if (ActionTimer > 60)
            {
                SetMotionAction(1);
            }
        }
        else
        {
            Rotation.Yaw += 0x100 * PlatformAngleUnitToDegrees;
            if (ActionTimer > 126)
            {
                SetMotionAction(0);
            }
        }
        break;

    case ESM64PlatformMotion::RotatingContinuous:
        // Pure periodic movers derive their pose from the integer course frame,
        // avoiding accumulated floating-point drift after long sessions/hitches.
        Rotation = HomeTransform.Rotator();
        Rotation.Yaw += FMath::Fmod(
            RotationDegreesPerFrame * static_cast<float>(SimulationFrame + 1),
            360.0f);
        break;

    case ESM64PlatformMotion::TowerSliding:
    {
        const int32 HalfCycle = FMath::FloorToInt(TravelDistance / FMath::Max(1.0f, SpeedPerFrame));
        const float Sign = MotionAction == 0 ? -1.0f : 1.0f;
        Location += Direction * SpeedPerFrame * Sign;
        if (ActionTimer > HalfCycle)
        {
            SetMotionAction(MotionAction == 0 ? 1 : 0);
        }
        break;
    }

    case ESM64PlatformMotion::TowerElevator:
        if (MotionAction == 1)
        {
            if (ActionTimer > 140)
            {
                SetMotionAction(2);
            }
            else
            {
                Location.Z += 5.0f;
            }
        }
        else if (MotionAction == 2)
        {
            if (ActionTimer > 60)
            {
                SetMotionAction(3);
            }
        }
        else if (MotionAction == 3)
        {
            if (ActionTimer > 140)
            {
                Location.Z = HomeTransform.GetLocation().Z;
                SetMotionAction(0);
            }
            else
            {
                Location.Z -= 5.0f;
            }
        }
        break;

    case ESM64PlatformMotion::TumblingPiece:
        if (MotionAction == 1)
        {
            FHitResult FloorHit;
            FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(SM64TumblingFloor), false, this);
            if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
                FloorHit,
                Location + FVector(0.0f, 0.0f, 10.0f),
                Location - FVector(0.0f, 0.0f, 10000.0f),
                ECC_WorldStatic,
                FloorQuery))
            {
                TumblingFloorHeight = FloorHit.ImpactPoint.Z;
            }
            if (ActionTimer > 5)
            {
                SetMotionAction(2);
                RollAcceleration = RandomStream.RandRange(0, 1) == 0
                    ? -0x80 * PlatformAngleUnitToDegrees
                    : 0x80 * PlatformAngleUnitToDegrees;
            }
        }
        else if (MotionAction == 2)
        {
            PitchVelocity = FMath::Min(
                PitchVelocity + 0x80 * PlatformAngleUnitToDegrees,
                0x400 * PlatformAngleUnitToDegrees);
            if (RollVelocity > -0x400 * PlatformAngleUnitToDegrees
                && RollVelocity < 0x400 * PlatformAngleUnitToDegrees)
            {
                RollVelocity += RollAcceleration;
            }
            VerticalVelocity -= 3.0f;
            Location.Z += VerticalVelocity;
            Rotation.Pitch += PitchVelocity;
            Rotation.Roll += RollVelocity;
            if (Location.Z < TumblingFloorHeight - 300.0f)
            {
                SetMotionAction(3);
            }
        }
        break;

    default:
        break;
    }

    Next.SetLocation(Location);
    Next.SetRotation(Rotation.Quaternion());
    ApplyPlatformTransform(Next);
    ++ActionTimer;
    bInsideSimulationStep = false;
}

void ASM64MovingPlatformBase::ApplyPlatformTransform(const FTransform& NewTransform)
{
    const FTransform OldTransform = GetActorTransform();
    SetActorTransform(NewTransform, false, nullptr, ETeleportType::None);

    if (!bManualRiderConveyance)
    {
        return;
    }

    for (int32 Index = Riders.Num() - 1; Index >= 0; --Index)
    {
        AActor* Rider = Riders[Index].Get();
        if (!IsValid(Rider))
        {
            Riders.RemoveAtSwap(Index);
            continue;
        }

        const FVector LocalPosition = OldTransform.InverseTransformPosition(Rider->GetActorLocation());
        const FVector CarriedPosition = NewTransform.TransformPosition(LocalPosition);
        const FQuat LocalRotation = OldTransform.GetRotation().Inverse() * Rider->GetActorQuat();
        const FQuat CarriedRotation = NewTransform.GetRotation() * LocalRotation;
        Rider->SetActorLocationAndRotation(CarriedPosition, CarriedRotation, false, nullptr, ETeleportType::None);
    }
}

void ASM64MovingPlatformBase::OnRiderBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this)
    {
        return;
    }
    Riders.AddUnique(OtherActor);
    if (Motion == ESM64PlatformMotion::TowerElevator && MotionAction == 0)
    {
        SetMotionAction(1);
    }
    else if (Motion == ESM64PlatformMotion::TumblingPiece && MotionAction == 0)
    {
        SetMotionAction(1);
    }
}

void ASM64MovingPlatformBase::OnRiderEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    Riders.Remove(OtherActor);
}
