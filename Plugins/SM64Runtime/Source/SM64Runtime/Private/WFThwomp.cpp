#include "WFThwomp.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AWFThwomp::AWFThwomp()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(SceneRoot);
    CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionBox->SetNotifyRigidBodyCollision(true);
    CollisionBox->OnComponentHit.AddDynamic(this, &AWFThwomp::OnThwompHit);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(SceneRoot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ExactCollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExactCollisionMesh"));
    ExactCollisionMesh->SetupAttachment(SceneRoot);
    ExactCollisionMesh->SetVisibility(false, true);
    ExactCollisionMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ExactCollisionMesh->SetNotifyRigidBodyCollision(true);
    ExactCollisionMesh->OnComponentHit.AddDynamic(this, &AWFThwomp::OnThwompHit);
}

void AWFThwomp::BeginPlay()
{
    HomeLocation = GetActorLocation() + FVector(0.0f, 0.0f, 1.0f);
    bHomeCaptured = true;
    Super::BeginPlay();
}

void AWFThwomp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CollisionBox->SetBoxExtent(BoxExtent);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
    UStaticMesh* SelectedCollision = bUseThwomp2Collision ? Thwomp2CollisionAsset : ThwompCollisionAsset;
    ExactCollisionMesh->SetStaticMesh(SelectedCollision);
    ExactCollisionMesh->SetCollisionEnabled(SelectedCollision
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    CollisionBox->SetCollisionEnabled(!SelectedCollision && bAllowProxyCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AWFThwomp::OnThwompHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor->IsA<APawn>()
        && (GetThwompAction() == EWFThwompAction::Lower || GetThwompAction() == EWFThwompAction::Land))
    {
        OnThwompPlayerContact(OtherActor, ContactDamage, true);
    }
}

void AWFThwomp::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation() + FVector(0.0f, 0.0f, 1.0f);
        bHomeCaptured = true;
    }
    SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
    UStaticMesh* SelectedCollision = bUseThwomp2Collision ? Thwomp2CollisionAsset : ThwompCollisionAsset;
    ExactCollisionMesh->SetCollisionEnabled(bActEnabled && SelectedCollision
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    CollisionBox->SetCollisionEnabled(bActEnabled && !SelectedCollision && bAllowProxyCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    ActionCode = static_cast<int32>(EWFThwompAction::Raise);
    ActionTimer = 0;
    VerticalVelocityPerFrame = 0.0f;
    RandomWaitFrames = 0;
}

EWFThwompAction AWFThwomp::GetThwompAction() const
{
    return static_cast<EWFThwompAction>(ActionCode);
}

void AWFThwomp::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;

    switch (GetThwompAction())
    {
        case EWFThwompAction::Raise:
            if (ActionTimer > RaiseDelayParameter + 40)
            {
                AddActorWorldOffset(FVector(0.0f, 0.0f, 5.0f), true);
                SetActionCode(static_cast<int32>(EWFThwompAction::IdleAtTop));
            }
            else
            {
                AddActorWorldOffset(FVector(0.0f, 0.0f, 10.0f), true);
            }
            break;

        case EWFThwompAction::IdleAtTop:
            if (ActionTimer == 0)
            {
                RandomWaitFrames = 10 + RandomIntegerExclusive(30); // [10, 39]
            }
            if (ActionTimer > RandomWaitFrames)
            {
                SetActionCode(static_cast<int32>(EWFThwompAction::Lower));
            }
            break;

        case EWFThwompAction::Lower:
        {
            VerticalVelocityPerFrame -= 4.0f;
            FVector NextLocation = GetActorLocation();
            NextLocation.Z += VerticalVelocityPerFrame;
            if (NextLocation.Z < HomeLocation.Z)
            {
                NextLocation.Z = HomeLocation.Z;
                VerticalVelocityPerFrame = 0.0f;
                SetActionCode(static_cast<int32>(EWFThwompAction::Land));
            }
            SetActorLocation(NextLocation, true);
            break;
        }

        case EWFThwompAction::Land:
            if (ActionTimer == 0)
            {
                const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
                const bool bNearPlayer = Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 1500.0f;
                OnThwompLanded(bNearPlayer);
            }
            if (ActionTimer >= 10)
            {
                SetActionCode(static_cast<int32>(EWFThwompAction::IdleAtBottom));
            }
            break;

        case EWFThwompAction::IdleAtBottom:
            if (ActionTimer == 0)
            {
                RandomWaitFrames = 20 + RandomIntegerExclusive(10); // [20, 29]
            }
            if (ActionTimer > RandomWaitFrames)
            {
                SetActionCode(static_cast<int32>(EWFThwompAction::Raise));
            }
            break;
    }

    FinishActionFrame(PreviousAction);
}
