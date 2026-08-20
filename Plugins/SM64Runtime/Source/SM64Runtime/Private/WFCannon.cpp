#include "WFCannon.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "SM64CourseManager.h"
#include "SM64PlayerAdapter.h"

AWFCannon::AWFCannon()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
    LidMesh->SetupAttachment(SceneRoot);
    LidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    LidCollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidCollisionMesh"));
    LidCollisionMesh->SetupAttachment(LidMesh);
    LidCollisionMesh->SetVisibility(false, true);
    LidCollisionMesh->SetCollisionProfileName(TEXT("BlockAll"));

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(SceneRoot);
    BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
    BarrelMesh->SetupAttachment(BaseMesh);
    BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(BaseMesh);
    InteractionSphere->SetSphereRadius(InteractionDistance);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWFCannon::OnInteractionOverlap);
}

void AWFCannon::BeginPlay()
{
    LidInitialRelativeTransform = LidMesh->GetRelativeTransform();
    BaseInitialRelativeTransform = BaseMesh->GetRelativeTransform();
    BarrelInitialRelativeTransform = BarrelMesh->GetRelativeTransform();
    bTransformsCaptured = true;
    Super::BeginPlay();
}

void AWFCannon::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InteractionSphere->SetSphereRadius(InteractionDistance);
    if (DefaultLidMesh)
    {
        LidMesh->SetStaticMesh(DefaultLidMesh);
    }
    LidCollisionMesh->SetStaticMesh(DefaultLidCollisionMesh);
    LidCollisionMesh->SetCollisionEnabled(DefaultLidCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    LidMesh->SetCollisionEnabled(!DefaultLidCollisionMesh && bAllowLidRenderCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (DefaultBaseMesh)
    {
        BaseMesh->SetStaticMesh(DefaultBaseMesh);
    }
    if (DefaultBarrelMesh)
    {
        BarrelMesh->SetStaticMesh(DefaultBarrelMesh);
    }
}

void AWFCannon::RestoreComponentTransforms()
{
    if (!bTransformsCaptured)
    {
        LidInitialRelativeTransform = LidMesh->GetRelativeTransform();
        BaseInitialRelativeTransform = BaseMesh->GetRelativeTransform();
        BarrelInitialRelativeTransform = BarrelMesh->GetRelativeTransform();
        bTransformsCaptured = true;
    }
    LidMesh->SetRelativeTransform(LidInitialRelativeTransform);
    BaseMesh->SetRelativeTransform(BaseInitialRelativeTransform);
    BarrelMesh->SetRelativeTransform(BarrelInitialRelativeTransform);
}

void AWFCannon::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    RestoreComponentTransforms();
    if (LoadedRider)
    {
        LoadedRider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        LoadedRider->SetActorEnableCollision(true);
        if (ACharacter* Character = Cast<ACharacter>(LoadedRider))
        {
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
    }
    LoadedRider = nullptr;
    AimPitchDegrees = 0.0f;
    AimYawOffsetDegrees = 0.0f;
    PresentationPhaseDegrees = 0.0f;

    const bool bUnlocked = IsCannonUnlocked();
    ActionCode = static_cast<int32>(bUnlocked ? EWFCannonState::OpenIdle : EWFCannonState::Closed);
    ActionTimer = 0;
    LidMesh->SetVisibility(!bUnlocked && bActEnabled, true);
    const bool bUseLidCollision = !bUnlocked && bActEnabled;
    LidCollisionMesh->SetCollisionEnabled(bUseLidCollision && DefaultLidCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    LidMesh->SetCollisionEnabled(bUseLidCollision && !DefaultLidCollisionMesh && bAllowLidRenderCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    SetCannonVisible(bUnlocked && bActEnabled);
}

EWFCannonState AWFCannon::GetCannonState() const
{
    return static_cast<EWFCannonState>(ActionCode);
}

bool AWFCannon::IsCannonUnlocked() const
{
    if (bStartUnlockedWithoutSave)
    {
        return true;
    }
    const ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this);
    return Manager && Manager->Progress.bCannonUnlocked;
}

void AWFCannon::MarkCannonUnlockedInProgress()
{
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        Manager->SetCannonUnlocked(true);
    }
}

void AWFCannon::EnterCannonState(EWFCannonState NewState)
{
    SetActionCode(static_cast<int32>(NewState));
    PresentationPhaseDegrees = 0.0f;
}

void AWFCannon::SetCannonVisible(bool bVisible)
{
    BaseMesh->SetVisibility(bVisible, true);
    BarrelMesh->SetVisibility(bVisible, true);
    // The render mesh is intentionally non-colliding. The decomp cannon has no
    // base collision array; interaction is handled by the dedicated sphere.
    BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionSphere->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AWFCannon::UnlockCannon(AActor* UnlockingActor)
{
    if (GetCannonState() != EWFCannonState::Closed)
    {
        return;
    }
    MarkCannonUnlockedInProgress();
    OnCannonUnlockStarted(UnlockingActor);
    EnterCannonState(EWFCannonState::UnlockCamera);
}

bool AWFCannon::RequestEnterCannon(AActor* RiderActor)
{
    if (!RiderActor || GetCannonState() != EWFCannonState::OpenIdle
        || FVector::Dist(RiderActor->GetActorLocation(), GetActorLocation()) >= InteractionDistance)
    {
        return false;
    }
    LoadedRider = RiderActor;
    const FVector SeatLocation = BaseMesh->GetComponentLocation() + FVector(0.0f, 0.0f, 350.0f);
    RiderActor->SetActorEnableCollision(false);
    if (ACharacter* Character = Cast<ACharacter>(RiderActor))
    {
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement();
    }
    RiderActor->AttachToComponent(BaseMesh, FAttachmentTransformRules::KeepWorldTransform);
    RiderActor->SetActorLocation(SeatLocation, false, nullptr, ETeleportType::TeleportPhysics);
    OnRequestAttachRider(RiderActor, SeatLocation);
    EnterCannonState(EWFCannonState::Raising);
    return true;
}

void AWFCannon::OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (bAutoEnterOnPawnOverlap && OtherActor && OtherActor->IsA<APawn>())
    {
        RequestEnterCannon(OtherActor);
    }
}

void AWFCannon::AddAimInput(float NormalizedYawInput, float NormalizedPitchInput)
{
    if (GetCannonState() != EWFCannonState::Aiming)
    {
        return;
    }
    // A full raw-stick deflection changes 800 SM64 angle units per second at 30 Hz.
    AimYawOffsetDegrees = FMath::Clamp(AimYawOffsetDegrees + NormalizedYawInput * 4.39453125f, -90.0f, 90.0f);
    AimPitchDegrees = FMath::Clamp(AimPitchDegrees + NormalizedPitchInput * 4.39453125f, 0.0f, 79.9977f);
    BarrelMesh->SetRelativeRotation(FRotator(AimPitchDegrees, InitialBarrelYawDegrees + AimYawOffsetDegrees, 0.0f));
    OnCannonAimChanged(AimPitchDegrees, GetActorRotation().Yaw + InitialBarrelYawDegrees + AimYawOffsetDegrees);
}

bool AWFCannon::LaunchLoadedRider()
{
    if (!LoadedRider || GetCannonState() != EWFCannonState::Aiming)
    {
        return false;
    }

    const float WorldYaw = GetActorRotation().Yaw + InitialBarrelYawDegrees + AimYawOffsetDegrees;
    const float PitchRadians = FMath::DegreesToRadians(AimPitchDegrees);
    const float StoredUEYawRadians = FMath::DegreesToRadians(WorldYaw);
    const FVector Direction(
        -FMath::Cos(PitchRadians) * FMath::Sin(StoredUEYawRadians),
        FMath::Cos(PitchRadians) * FMath::Cos(StoredUEYawRadians),
        FMath::Sin(PitchRadians));
    const FVector LaunchLocation = BaseMesh->GetComponentLocation() + Direction * LaunchMuzzleOffset;
    const FVector LaunchVelocity = Direction * LaunchSpeedPerFrame * SimulationRate;
    AActor* RiderToLaunch = LoadedRider;
    LoadedRider = nullptr;
    RiderToLaunch->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RiderToLaunch->SetActorLocation(LaunchLocation, false, nullptr, ETeleportType::TeleportPhysics);
    RiderToLaunch->SetActorEnableCollision(true);
    if (ACharacter* Character = Cast<ACharacter>(RiderToLaunch))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Character->LaunchCharacter(LaunchVelocity, true, true);
    }
    else if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(RiderToLaunch->GetRootComponent()))
    {
        RootPrimitive->SetPhysicsLinearVelocity(LaunchVelocity);
    }
    for (TActorIterator<ASM64PlayerAdapter> It(GetWorld()); It; ++It)
    {
        if (It->SpyroActor == RiderToLaunch)
        {
            It->SetCannonLaunched(true);
        }
    }
    OnRequestLaunchRider(RiderToLaunch, LaunchLocation, LaunchVelocity);
    EnterCannonState(EWFCannonState::PostLaunch);
    return true;
}

void AWFCannon::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;

    switch (GetCannonState())
    {
        case EWFCannonState::Closed:
            break;

        case EWFCannonState::UnlockCamera:
            if (ActionTimer == 0)
            {
                OnCannonUnlockCameraFrame();
            }
            if (ActionTimer == 60)
            {
                EnterCannonState(EWFCannonState::OpeningLid);
            }
            break;

        case EWFCannonState::OpeningLid:
            if (ActionTimer == 0)
            {
                OnCannonPresentationSound(0);
            }
            if (ActionTimer < 30)
            {
                LidMesh->AddRelativeLocation(FVector(0.0f, 0.0f, -0.5f));
            }
            else if (ActionTimer == 80)
            {
                LidMesh->SetVisibility(false, true);
                LidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                LidCollisionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SetCannonVisible(true);
                OnCannonOpened();
                EnterCannonState(EWFCannonState::OpenIdle);
            }
            else
            {
                LidMesh->AddRelativeLocation(FVector(4.0f, 0.0f, 0.0f));
            }
            break;

        case EWFCannonState::OpenIdle:
        {
            bool bVisible = true;
            if (bHideOpenCannonBeyondInteractionDistance)
            {
                const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
                bVisible = Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < InteractionDistance;
            }
            SetCannonVisible(bVisible);
            break;
        }

        case EWFCannonState::Raising:
        {
            if (ActionTimer == 0)
            {
                OnCannonPresentationSound(1);
            }
            BaseMesh->AddRelativeLocation(FVector(
                static_cast<float>((ActionTimer / 2 & 1)) - 0.5f,
                static_cast<float>((ActionTimer / 2 & 1)) - 0.5f,
                5.0f));
            if (ActionTimer > 67)
            {
                BaseMesh->AddRelativeLocation(FVector(
                    (static_cast<float>((ActionTimer / 2 & 1)) - 0.5f) * 2.0f,
                    (static_cast<float>((ActionTimer / 2 & 1)) - 0.5f) * 2.0f,
                    0.0f));
                EnterCannonState(EWFCannonState::YawPresentation);
            }
            break;
        }

        case EWFCannonState::YawPresentation:
            if (ActionTimer == 0)
            {
                OnCannonPresentationSound(2);
            }
            if (ActionTimer < 4)
            {
                BaseMesh->AddRelativeLocation(FVector(
                    (static_cast<float>((ActionTimer / 2 & 1)) - 0.5f) * 2.0f,
                    (static_cast<float>((ActionTimer / 2 & 1)) - 0.5f) * 2.0f,
                    0.0f));
            }
            else if (ActionTimer >= 6 && ActionTimer < 22)
            {
                BarrelMesh->SetRelativeRotation(FRotator(0.0f,
                    InitialBarrelYawDegrees - FMath::Sin(FMath::DegreesToRadians(PresentationPhaseDegrees)) * 90.0f,
                    0.0f));
                PresentationPhaseDegrees += 5.625f; // +0x400
            }
            else if (ActionTimer >= 26)
            {
                EnterCannonState(EWFCannonState::PitchPresentation);
            }
            break;

        case EWFCannonState::PitchPresentation:
            if (ActionTimer == 0)
            {
                OnCannonPresentationSound(3);
            }
            if (ActionTimer >= 4 && ActionTimer < 20)
            {
                PresentationPhaseDegrees += 5.625f;
                BarrelMesh->SetRelativeRotation(FRotator(
                    FMath::Sin(FMath::DegreesToRadians(PresentationPhaseDegrees)) * 45.0f,
                    InitialBarrelYawDegrees,
                    0.0f));
            }
            else if (ActionTimer >= 25)
            {
                AimPitchDegrees = 0.0f;
                AimYawOffsetDegrees = 0.0f;
                EnterCannonState(EWFCannonState::Aiming);
                OnCannonAimReady(LoadedRider);
            }
            break;

        case EWFCannonState::Aiming:
            if (bReadDirectPlayerInput)
            {
                if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
                {
                    float MouseX = 0.0f;
                    float MouseY = 0.0f;
                    Controller->GetInputMouseDelta(MouseX, MouseY);
                    const float YawInput = FMath::Clamp(
                        Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftX) + MouseX * 0.04f,
                        -1.0f, 1.0f);
                    const float PitchInput = FMath::Clamp(
                        Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftY) - MouseY * 0.04f,
                        -1.0f, 1.0f);
                    AddAimInput(YawInput, PitchInput);
                    if (Controller->WasInputKeyJustPressed(EKeys::SpaceBar)
                        || Controller->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
                    {
                        LaunchLoadedRider();
                    }
                }
            }
            break;

        case EWFCannonState::PostLaunch:
            EnterCannonState(EWFCannonState::ResetAfterLaunch);
            break;

        case EWFCannonState::ResetAfterLaunch:
            if (ActionTimer > 3)
            {
                RestoreComponentTransforms();
                SetCannonVisible(true);
                EnterCannonState(EWFCannonState::OpenIdle);
            }
            break;
    }

    FinishActionFrame(PreviousAction);
}
