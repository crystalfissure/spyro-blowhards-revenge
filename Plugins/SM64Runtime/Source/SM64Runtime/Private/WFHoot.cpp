#include "WFHoot.h"

#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AWFHoot::AWFHoot()
{
    ActMask = 0x3C; // Acts 3-6

    WakeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("WakeSphere"));
    RootComponent = WakeSphere;
    WakeSphere->SetSphereRadius(WakeRadius);
    WakeSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WakeSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    WakeSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    WakeSphere->OnComponentBeginOverlap.AddDynamic(this, &AWFHoot::OnWakeOverlap);

    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(WakeSphere);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWFHoot::BeginPlay()
{
    SpawnLocation = GetActorLocation();
    SpawnRotation = GetActorRotation();
    HomeLocation = SpawnLocation + HomeOffset;
    bHomeCaptured = true;
    Super::BeginPlay();
}

void AWFHoot::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    WakeSphere->SetSphereRadius(WakeRadius);
    if (DefaultSkeletalMesh)
    {
        CharacterMesh->SetSkeletalMesh(DefaultSkeletalMesh);
    }
}

void AWFHoot::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (Rider)
    {
        Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Rider->SetActorEnableCollision(true);
        if (ACharacter* Character = Cast<ACharacter>(Rider))
        {
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
    }
    if (!bHomeCaptured)
    {
        SpawnLocation = GetActorLocation();
        SpawnRotation = GetActorRotation();
        HomeLocation = SpawnLocation + HomeOffset;
        bHomeCaptured = true;
    }
    SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    ActionCode = static_cast<int32>(EWFHootState::AsleepInTree);
    ActionTimer = 0;
    Rider = nullptr;
    FlightPitchDegrees = 0.0f;
    SteeringInput = 0.0f;
    bIntroDialogRequested = false;
    bTiredDialogRequested = false;
    CharacterMesh->SetVisibility(false, true);
    if (FreeFlightAnimation)
    {
        CharacterMesh->PlayAnimation(FreeFlightAnimation, true);
    }
    WakeSphere->SetCollisionEnabled(bActEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

EWFHootState AWFHoot::GetHootState() const
{
    return static_cast<EWFHootState>(ActionCode);
}

void AWFHoot::EnterHootState(EWFHootState NewState)
{
    SetActionCode(static_cast<int32>(NewState));
}

float AWFHoot::FindFloorHeightAt(const FVector& WorldLocation) const
{
    if (!GetWorld())
    {
        return -BIG_NUMBER;
    }
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WFHootFloor), false, this);
    const FVector Start(WorldLocation.X, WorldLocation.Y, 10000.0f);
    const FVector End(WorldLocation.X, WorldLocation.Y, -10000.0f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FloorTraceChannel, Params))
    {
        return Hit.ImpactPoint.Z;
    }
    return -BIG_NUMBER;
}

void AWFHoot::TurnTowardPoint(const FVector& Point, float MaxYawDegreesPerFrame, float MaxPitchDegreesPerFrame)
{
    const FVector Delta = Point - GetActorLocation();
    const float HorizontalDistance = FVector2D(Delta.X, Delta.Y).Size();
    const float DesiredYaw = -FMath::RadiansToDegrees(FMath::Atan2(Delta.X, Delta.Y));
    // SM64 stores positive pitch as downward for Hoot's movement formula.
    const float DesiredPitch = -FMath::RadiansToDegrees(FMath::Atan2(Delta.Z, HorizontalDistance));
    FRotator Rotation = GetActorRotation();
    Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, DesiredYaw, MaxYawDegreesPerFrame);
    SetActorRotation(Rotation);
    FlightPitchDegrees = FMath::FixedTurn(FlightPitchDegrees, DesiredPitch, MaxPitchDegreesPerFrame);
}

void AWFHoot::MoveHoot(float SpeedPerFrame, bool bFastWingOscillation)
{
    const FVector PreviousLocation = GetActorLocation();
    const float PitchRadians = FMath::DegreesToRadians(FlightPitchDegrees);
    const float HorizontalSpeed = FMath::Cos(PitchRadians) * SpeedPerFrame;
    const FVector Forward = GetSM64ForwardVector();
    const int32 WingFrame = static_cast<int32>(SimulationFrame % (bFastWingOscillation ? 10 : 20));
    const float WingAngle = static_cast<float>(WingFrame) * (bFastWingOscillation ? 36.0f : 18.0f);
    const float WingBob = FMath::Cos(FMath::DegreesToRadians(WingAngle)) * 12.5f;
    FVector Delta = Forward * HorizontalSpeed;
    Delta.Z = -FMath::Sin(PitchRadians) * SpeedPerFrame - WingBob;
    SetActorLocation(PreviousLocation + Delta, true);
    ApplyFloorAndWorldBounds(PreviousLocation);
}

void AWFHoot::ApplyFloorAndWorldBounds(const FVector& PreviousLocation)
{
    FVector Location = GetActorLocation();
    if (FMath::Abs(Location.X) > 8000.0f)
    {
        Location.X = PreviousLocation.X;
    }
    if (FMath::Abs(Location.Y) > 8000.0f)
    {
        Location.Y = PreviousLocation.Y;
    }
    const float FloorHeight = FindFloorHeightAt(Location);
    if (FloorHeight > -BIG_NUMBER / 2.0f && Location.Z < FloorHeight + 125.0f)
    {
        Location.Z = FloorHeight + 125.0f;
    }
    SetActorLocation(Location, true);
}

void AWFHoot::PollNativeSteeringInput()
{
    if (!bPollNativeSteeringInput)
    {
        return;
    }
    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (!Controller)
    {
        return;
    }
    float Input = Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
    if (Controller->IsInputKeyDown(EKeys::A) || Controller->IsInputKeyDown(EKeys::Left))
    {
        Input -= 1.0f;
    }
    if (Controller->IsInputKeyDown(EKeys::D) || Controller->IsInputKeyDown(EKeys::Right))
    {
        Input += 1.0f;
    }
    SteeringInput = FMath::Clamp(Input, -1.0f, 1.0f);
}

void AWFHoot::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;

    switch (GetHootState())
    {
        case EWFHootState::AsleepInTree:
            break;

        case EWFHootState::WantsToTalk:
        case EWFHootState::ReadyToFly:
        {
            TurnTowardPoint(HomeLocation, 1.7578125f, 1.7578125f); // 0x140/frame

            const FVector Forward = GetSM64ForwardVector();
            const float Floor375 = FindFloorHeightAt(GetActorLocation() + Forward * 375.0f);
            if (Floor375 + 75.0f > GetActorLocation().Z)
            {
                FlightPitchDegrees -= 20.0f;
            }
            const float Floor200 = FindFloorHeightAt(GetActorLocation() + Forward * 200.0f);
            if (Floor200 + 125.0f > GetActorLocation().Z)
            {
                FlightPitchDegrees -= 40.0f;
            }
            FlightPitchDegrees = FMath::Max(-120.0f, FlightPitchDegrees);
            MoveHoot(10.0f, false);
            break;
        }

        case EWFHootState::Ascent:
            TurnTowardPoint(CourseOrigin, 7.03125f, 0.0f); // yaw approaches by 0x500
            FlightPitchDegrees = -70.0f; // signed 0xCE38
            if (ActionTimer == 29)
            {
                OnHootWindStarted();
            }
            MoveHoot(60.0f, true);
            if (GetActorLocation().Z > AscentTargetHeight)
            {
                EnterHootState(EWFHootState::Carry);
            }
            break;

        case EWFHootState::Carry:
            PollNativeSteeringInput();
            AddActorWorldRotation(FRotator(0.0f, SteeringInput * 2.197265625f, 0.0f)); // source yaw subtracts; stored UE yaw is negated
            FlightPitchDegrees = 10.0f; // 0x071C
            MoveHoot(20.0f, true);
            if (Rider)
            {
                OnHootCarryTransform(Rider, GetActorTransform());
            }
            if (GetActorLocation().Z < TiredDialogHeight && !bTiredDialogRequested)
            {
                bTiredDialogRequested = true;
                OnRequestHootDialog(45);
                if (bAutoCompleteTiredDialog)
                {
                    CompleteTiredDialog();
                }
            }
            break;

        case EWFHootState::Tired:
            PollNativeSteeringInput();
            AddActorWorldRotation(FRotator(0.0f, SteeringInput * 2.197265625f, 0.0f));
            FlightPitchDegrees = 0.0f;
            MoveHoot(20.0f, true);
            if (Rider)
            {
                OnHootCarryTransform(Rider, GetActorTransform());
            }
            if (ActionTimer > 60)
            {
                ReleaseRider();
            }
            break;
    }

    FinishActionFrame(PreviousAction);
}

void AWFHoot::CompleteIntroDialog()
{
    if (GetHootState() == EWFHootState::WantsToTalk)
    {
        EnterHootState(EWFHootState::ReadyToFly);
    }
}

bool AWFHoot::AttachRider(AActor* RiderActor)
{
    if (!RiderActor || (GetHootState() != EWFHootState::ReadyToFly
        && GetHootState() != EWFHootState::WantsToTalk))
    {
        return false;
    }
    Rider = RiderActor;
    if (CarryAnimation)
    {
        CharacterMesh->PlayAnimation(CarryAnimation, true);
    }
    RiderActor->SetActorEnableCollision(false);
    if (ACharacter* Character = Cast<ACharacter>(RiderActor))
    {
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement();
    }
    RiderActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    RiderActor->SetActorRelativeLocation(RiderRelativeOffset);
    OnRequestAttachToHoot(RiderActor);
    EnterHootState(EWFHootState::Ascent);
    return true;
}

void AWFHoot::SetSteeringInput(float NormalizedInput)
{
    SteeringInput = FMath::Clamp(NormalizedInput, -1.0f, 1.0f);
}

void AWFHoot::CompleteTiredDialog()
{
    if (GetHootState() == EWFHootState::Carry && bTiredDialogRequested)
    {
        EnterHootState(EWFHootState::Tired);
    }
}

void AWFHoot::ReleaseRider()
{
    if (!Rider)
    {
        return;
    }
    AActor* ReleasedRider = Rider;
    Rider = nullptr;
    ReleasedRider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    ReleasedRider->SetActorEnableCollision(true);
    if (ACharacter* Character = Cast<ACharacter>(ReleasedRider))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
    OnRequestReleaseFromHoot(ReleasedRider);
    if (FreeFlightAnimation)
    {
        CharacterMesh->PlayAnimation(FreeFlightAnimation, true);
    }
    EnterHootState(EWFHootState::ReadyToFly);
}

void AWFHoot::OnWakeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && GetHootState() == EWFHootState::AsleepInTree)
    {
        CharacterMesh->SetVisibility(true, true);
        EnterHootState(EWFHootState::WantsToTalk);
        if (!bIntroDialogRequested)
        {
            bIntroDialogRequested = true;
            OnRequestHootDialog(44);
            if (bAutoCompleteIntroDialog)
            {
                CompleteIntroDialog();
            }
            if (bAutoAttachOnWakeOverlap)
            {
                AttachRider(OtherActor);
            }
        }
    }
}
