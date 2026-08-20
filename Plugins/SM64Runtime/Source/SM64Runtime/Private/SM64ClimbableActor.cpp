#include "SM64ClimbableActor.h"

#include "GameFramework/Pawn.h"

ASM64ClimbableActor::ASM64ClimbableActor()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    ClimbVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ClimbVolume"));
    ClimbVolume->SetupAttachment(SceneRoot);
    ClimbVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ClimbVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    ClimbVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ClimbVolume->OnComponentBeginOverlap.AddDynamic(this, &ASM64ClimbableActor::OnClimbVolumeBegin);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(SceneRoot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASM64ClimbableActor::BeginPlay()
{
    BaseLocation = GetActorLocation();
    bBaseCaptured = true;
    Super::BeginPlay();
}

void ASM64ClimbableActor::ApplyVanillaMetadata()
{
    ClimbRadius = 80.0f;
    TopClearance = 100.0f;
    LowerInteractionPadding = 10.0f;
    UpperInteractionPadding = 30.0f;
    PushRadius = 70.0f;
    PushDelayFrames = 10;

    switch (ClimbableType)
    {
        case ESM64ClimbableType::Tree:
            ClimbHeight = 500.0f;
            bAllowTopTransition = true;
            break;
        case ESM64ClimbableType::GenericPole:
            ClimbHeight = static_cast<float>(BehaviorHeightByte * 10);
            bAllowTopTransition = true;
            break;
        case ESM64ClimbableType::GiantWFPole:
            ClimbHeight = 2100.0f;
            bAllowTopTransition = false;
            break;
    }
    UpdateVolumeFromMetadata();
}

void ASM64ClimbableActor::UpdateVolumeFromMetadata()
{
    const float TotalHeight = ClimbHeight + LowerInteractionPadding + UpperInteractionPadding;
    const float HalfHeight = FMath::Max(ClimbRadius, TotalHeight * 0.5f);
    ClimbVolume->SetCapsuleSize(ClimbRadius, HalfHeight);
    ClimbVolume->SetRelativeLocation(FVector(0.0f, 0.0f,
        (ClimbHeight + UpperInteractionPadding - LowerInteractionPadding) * 0.5f));
}

void ASM64ClimbableActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (bApplyVanillaMetadataOnConstruction)
    {
        ApplyVanillaMetadata();
    }
    else
    {
        UpdateVolumeFromMetadata();
    }
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
}

void ASM64ClimbableActor::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bBaseCaptured)
    {
        BaseLocation = GetActorLocation();
        bBaseCaptured = true;
    }
    Climber = nullptr;
    ClimbPosition = 0.0f;
    VerticalClimbInput = 0.0f;
    HorizontalClimbInput = 0.0f;
    bTopNotified = false;
    ActionCode = static_cast<int32>(ESM64ClimbState::Available);
    ActionTimer = 0;
    if (ClimbableType == ESM64ClimbableType::GiantWFPole && bActEnabled)
    {
        OnGiantPoleTopBallRequested(BaseLocation + FVector(0.0f, 0.0f, ClimbHeight + 50.0f));
    }
}

bool ASM64ClimbableActor::CanAttachClimber(AActor* Candidate) const
{
    if (!Candidate || Climber || !bActEnabled)
    {
        return false;
    }
    const FVector Delta = Candidate->GetActorLocation() - BaseLocation;
    return FVector2D(Delta.X, Delta.Y).Size() <= ClimbRadius
        && Delta.Z > -LowerInteractionPadding
        && Delta.Z < ClimbHeight + UpperInteractionPadding;
}

bool ASM64ClimbableActor::AttachClimber(AActor* NewClimber)
{
    if (!CanAttachClimber(NewClimber))
    {
        return false;
    }
    Climber = NewClimber;
    ClimbPosition = FMath::Clamp(NewClimber->GetActorLocation().Z - BaseLocation.Z,
        -HitboxDownOffset, ClimbHeight - TopClearance);
    ActionCode = static_cast<int32>(ESM64ClimbState::Holding);
    ActionTimer = 0;
    OnRequestAttachToClimbable(NewClimber,
        BaseLocation + FVector(0.0f, 0.0f, ClimbPosition), ClimbPosition);
    return true;
}

void ASM64ClimbableActor::SetClimbInput(float NormalizedVertical, float NormalizedHorizontal)
{
    VerticalClimbInput = FMath::Clamp(NormalizedVertical, -1.0f, 1.0f);
    HorizontalClimbInput = FMath::Clamp(NormalizedHorizontal, -1.0f, 1.0f);
}

void ASM64ClimbableActor::SimulateSM64Frame_Implementation()
{
    if (!Climber)
    {
        return;
    }

    const int32 PreviousAction = ActionCode;
    if (VerticalClimbInput > 0.1f)
    {
        SetActionCode(static_cast<int32>(ESM64ClimbState::Climbing));
        ClimbPosition += VerticalClimbInput * 10.0f; // rawStickY max 80 divided by 8
        if (ClimbableType == ESM64ClimbableType::Tree)
        {
            OnTreeLeafEffect(Climber);
        }
    }
    else if (VerticalClimbInput < -0.2f)
    {
        SetActionCode(static_cast<int32>(ESM64ClimbState::Holding));
        ClimbPosition += VerticalClimbInput * 16.0f;
        if (ClimbableType == ESM64ClimbableType::Tree)
        {
            OnTreeLeafEffect(Climber);
        }
    }
    else
    {
        SetActionCode(static_cast<int32>(ESM64ClimbState::Holding));
    }

    const float PoleTop = ClimbHeight - TopClearance;
    ClimbPosition = FMath::Clamp(ClimbPosition, -HitboxDownOffset, PoleTop);
    const float FacingYawDelta = -HorizontalClimbInput * 7.03125f; // rawStickX*16 at max 80
    const FVector AttachLocation = BaseLocation + FVector(0.0f, 0.0f, ClimbPosition);
    OnClimbTransformUpdated(Climber, AttachLocation, ClimbPosition, FacingYawDelta);

    if (ClimbPosition >= PoleTop - 0.4f && !bTopNotified)
    {
        bTopNotified = true;
        SetActionCode(static_cast<int32>(ESM64ClimbState::AtTop));
        OnReachedClimbableTop(Climber, BaseLocation + FVector(0.0f, 0.0f, PoleTop), bAllowTopTransition);
    }
    else if (ClimbPosition < PoleTop - 0.4f)
    {
        bTopNotified = false;
    }

    if (ClimbPosition <= -HitboxDownOffset && VerticalClimbInput < 0.0f)
    {
        DetachClimber(true);
    }
    FinishActionFrame(PreviousAction);
}

void ASM64ClimbableActor::JumpOffClimbable()
{
    if (!Climber)
    {
        return;
    }
    AActor* Released = Climber;
    Climber = nullptr;
    SetActionCode(static_cast<int32>(ESM64ClimbState::Available));
    OnRequestDetachFromClimbable(Released, true, false);
}

void ASM64ClimbableActor::DetachClimber(bool bReachedFloor)
{
    if (!Climber)
    {
        return;
    }
    AActor* Released = Climber;
    Climber = nullptr;
    SetActionCode(static_cast<int32>(ESM64ClimbState::Available));
    OnRequestDetachFromClimbable(Released, false, bReachedFloor);
}

void ASM64ClimbableActor::OnClimbVolumeBegin(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && CanAttachClimber(OtherActor))
    {
        OnClimbableContactCandidate(OtherActor, OtherActor->GetActorLocation().Z - BaseLocation.Z);
    }
}
