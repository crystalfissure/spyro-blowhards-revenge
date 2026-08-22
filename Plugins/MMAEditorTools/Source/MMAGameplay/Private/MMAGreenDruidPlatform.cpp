#include "MMAGreenDruidPlatform.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"

AMMAGreenDruidPlatform::AMMAGreenDruidPlatform()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    PlatformVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Platform Visual"));
    PlatformVisual->SetupAttachment(SceneRoot);
    PlatformVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlatformVisual->SetGenerateOverlapEvents(false);
    PlatformVisual->SetMobility(EComponentMobility::Movable);

    RideSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("Ride Surface"));
    RideSurface->SetupAttachment(SceneRoot);
    RideSurface->SetMobility(EComponentMobility::Movable);
    RideSurface->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    RideSurface->SetGenerateOverlapEvents(false);
    RideSurface->SetBoxExtent(RideSurfaceBoxExtent);

    ColumnBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Column Blocker"));
    ColumnBlocker->SetupAttachment(SceneRoot);
    ColumnBlocker->SetMobility(EComponentMobility::Movable);
    ColumnBlocker->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    ColumnBlocker->SetGenerateOverlapEvents(false);

    PayloadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Payload Root"));
    PayloadRoot->SetupAttachment(SceneRoot);
    PayloadRoot->SetMobility(EComponentMobility::Movable);
}

void AMMAGreenDruidPlatform::BeginPlay()
{
    Super::BeginPlay();

    ForceFlat();
    if (PlatformVisual && LiftBoneName != NAME_None && PlatformVisual->GetBoneIndex(LiftBoneName) != INDEX_NONE)
    {
        bHasValidLiftBone = true;
        FlatLiftBoneWorldZ = PlatformVisual->GetSocketLocation(LiftBoneName).Z;
    }
    else
    {
        bHasValidLiftBone = false;
        if (LiftBoneName != NAME_None)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Green Druid platform %s cannot find lift bone '%s'; using %.1f cm fallback."),
                *GetName(), *LiftBoneName.ToString(), FallbackLiftHeight);
        }
    }
    UpdatePhysicalGeometry();
    AttachLinkedPayloads();
}

bool AMMAGreenDruidPlatform::ClaimController(UObject* NewController)
{
    if (!IsValid(NewController))
    {
        return false;
    }
    if (!Controller.IsValid() || Controller.Get() == NewController)
    {
        Controller = NewController;
        return true;
    }
    if (!bOwnershipWarningIssued)
    {
        bOwnershipWarningIssued = true;
        UE_LOG(LogTemp, Warning,
            TEXT("Green Druid platform %s rejected controller %s; it is already controlled by %s."),
            *GetName(), *NewController->GetName(), *Controller->GetName());
    }
    return false;
}

void AMMAGreenDruidPlatform::ReleaseController(UObject* ReleasingController)
{
    if (Controller.Get() == ReleasingController)
    {
        Controller.Reset();
    }
}

void AMMAGreenDruidPlatform::SetLiftAlpha(float NewLiftAlpha)
{
    LiftAlpha = FMath::Clamp(NewLiftAlpha, 0.0f, 1.0f);
    EvaluateVisualPose(LiftAlpha);
    UpdatePhysicalGeometry();
}

void AMMAGreenDruidPlatform::ForceFlat()
{
    SetLiftAlpha(0.0f);
}

bool AMMAGreenDruidPlatform::IsFlat() const
{
    return LiftAlpha <= KINDA_SMALL_NUMBER;
}

void AMMAGreenDruidPlatform::EvaluateVisualPose(float Alpha)
{
    if (!PlatformVisual || !LiftAnimation)
    {
        return;
    }
    if (PlatformVisual->GetAnimationMode() != EAnimationMode::AnimationSingleNode ||
        !PlatformVisual->GetSingleNodeInstance() ||
        PlatformVisual->GetSingleNodeInstance()->GetAnimationAsset() != LiftAnimation)
    {
        PlatformVisual->PlayAnimation(LiftAnimation, false);
    }
    if (UAnimSingleNodeInstance* Instance = PlatformVisual->GetSingleNodeInstance())
    {
        Instance->SetPlaying(false);
        Instance->SetPosition(FMath::Clamp(Alpha, 0.0f, 1.0f) * LiftAnimation->GetPlayLength(), false);
    }
    PlatformVisual->TickAnimation(0.0f, false);
    PlatformVisual->RefreshBoneTransforms();
    PlatformVisual->UpdateComponentToWorld();
}

void AMMAGreenDruidPlatform::UpdatePhysicalGeometry()
{
    if (!RideSurface || !ColumnBlocker || !PayloadRoot)
    {
        return;
    }

    float LiftDistance = FallbackLiftHeight * LiftAlpha;
    if (bHasValidLiftBone && PlatformVisual)
    {
        LiftDistance = PlatformVisual->GetSocketLocation(LiftBoneName).Z - FlatLiftBoneWorldZ;
    }

    const FVector SurfaceLocation = FlatSurfaceRelativeLocation + FVector::UpVector * LiftDistance;
    RideSurface->SetBoxExtent(RideSurfaceBoxExtent);
    RideSurface->SetRelativeLocation(SurfaceLocation);
    PayloadRoot->SetRelativeLocation(SurfaceLocation);

    const float FlatUnderside = FlatSurfaceRelativeLocation.Z - RideSurfaceBoxExtent.Z;
    const float CurrentUnderside = SurfaceLocation.Z - RideSurfaceBoxExtent.Z;
    const float ColumnHeight = FMath::Max(1.0f, CurrentUnderside - FlatUnderside);
    ColumnBlocker->SetBoxExtent(FVector(
        RideSurfaceBoxExtent.X,
        RideSurfaceBoxExtent.Y,
        ColumnHeight * 0.5f));
    ColumnBlocker->SetRelativeLocation(FVector(
        SurfaceLocation.X,
        SurfaceLocation.Y,
        FlatUnderside + ColumnHeight * 0.5f));
    ColumnBlocker->SetCollisionEnabled(
        ColumnHeight > 1.0f + KINDA_SMALL_NUMBER
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision);
}

void AMMAGreenDruidPlatform::AttachLinkedPayloads()
{
    for (AActor* Payload : LinkedPayloadActors)
    {
        if (!IsValid(Payload) || Payload == this)
        {
            continue;
        }
        Payload->AttachToComponent(
            PayloadRoot,
            FAttachmentTransformRules::KeepWorldTransform);
    }
}

