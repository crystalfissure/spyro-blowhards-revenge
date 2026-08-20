#include "WFBulletBill.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

AWFBulletBill::AWFBulletBill()
{
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionSphere->SetNotifyRigidBodyCollision(true);
    CollisionSphere->OnComponentHit.AddDynamic(this, &AWFBulletBill::OnBillHit);
    CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWFBulletBill::OnBillOverlap);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(CollisionSphere);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWFBulletBill::BeginPlay()
{
    HomeLocation = GetActorLocation();
    HomeRotation = GetActorRotation();
    bHomeCaptured = true;
    Super::BeginPlay();
}

void AWFBulletBill::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CollisionSphere->SetSphereRadius(CollisionRadius);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
}

void AWFBulletBill::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation();
        HomeRotation = GetActorRotation();
        bHomeCaptured = true;
    }
    ActionCode = static_cast<int32>(EWFBulletBillAction::Reset);
    ActionTimer = 0;
    bHitWallThisFrame = false;
    SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
    CollisionSphere->SetCollisionEnabled(bActEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    SetActorHiddenInGame(!bActEnabled);
}

EWFBulletBillAction AWFBulletBill::GetBulletBillAction() const
{
    return static_cast<EWFBulletBillAction>(ActionCode);
}

bool AWFBulletBill::CanSeePlayer(const APawn* Player) const
{
    if (!Player)
    {
        return false;
    }

    FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    ToPlayer.Z = 0.0f;
    const float Distance = ToPlayer.Size();
    if (Distance <= ActivationMinDistance || Distance >= ActivationMaxDistance || Distance <= SMALL_NUMBER)
    {
        return false;
    }
    ToPlayer /= Distance;
    const FVector Forward = GetSM64ForwardVector();
    return FVector::DotProduct(Forward, ToPlayer) > FMath::Cos(FMath::DegreesToRadians(ActivationHalfAngleDegrees));
}

void AWFBulletBill::TurnTowardPlayer(float MaxDegreesPerFrame)
{
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        return;
    }
    TurnSM64YawTowardLocation(Player->GetActorLocation(), MaxDegreesPerFrame);
}

void AWFBulletBill::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

    switch (GetBulletBillAction())
    {
        case EWFBulletBillAction::Reset:
            SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
            CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SetActorHiddenInGame(false);
            SetActionCode(static_cast<int32>(EWFBulletBillAction::WaitForPlayer));
            break;

        case EWFBulletBillAction::WaitForPlayer:
            if (CanSeePlayer(Player))
            {
                SetActionCode(static_cast<int32>(EWFBulletBillAction::Launch));
            }
            break;

        case EWFBulletBillAction::Launch:
            if (ActionTimer < 40)
            {
                AddActorWorldOffset(GetSM64ForwardVector() * 3.0f, true);
            }
            else if (ActionTimer < 50)
            {
                const float JitterSpeed = (ActionTimer & 1) ? 3.0f : -3.0f;
                AddActorWorldOffset(GetSM64ForwardVector() * JitterSpeed, true);
            }
            else
            {
                OnSpawnSmokePuff();
                if (Player && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) > 300.0f)
                {
                    TurnTowardPlayer(1.40625f); // 0x100 angle units/frame
                }
                AddActorWorldOffset(GetSM64ForwardVector() * 30.0f, true);
                if (ActionTimer == 50)
                {
                    OnCannonFired();
                }
                if (ActionTimer > 150 || (ActionTimer > 70 && bHitWallThisFrame))
                {
                    OnBulletBillImpact(nullptr);
                    SetActionCode(static_cast<int32>(EWFBulletBillAction::ResetAfterLaunch));
                }
            }
            bHitWallThisFrame = false;
            break;

        case EWFBulletBillAction::ResetAfterLaunch:
            SetActionCode(static_cast<int32>(EWFBulletBillAction::Reset));
            break;

        case EWFBulletBillAction::KnockedBack:
            AddActorWorldOffset(GetSM64ForwardVector() * -30.0f + FVector(0.0f, 0.0f, 20.0f), true);
            AddActorLocalRotation(FRotator(22.5f, 0.0f, 22.5f)); // 0x1000 on pitch and roll
            if (ActionTimer > 90)
            {
                SetActionCode(static_cast<int32>(EWFBulletBillAction::Reset));
            }
            break;
    }

    FinishActionFrame(PreviousAction);
}

bool AWFBulletBill::HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
    FVector ImpactPoint, FVector ImpactDirection)
{
    if (GetBulletBillAction() == EWFBulletBillAction::KnockedBack)
    {
        return false;
    }
    KnockBack(InstigatorActor);
    return true;
}

void AWFBulletBill::KnockBack(AActor* InstigatorActor)
{
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetActionCode(static_cast<int32>(EWFBulletBillAction::KnockedBack));
    OnBulletBillKnockedBack(InstigatorActor);
}

void AWFBulletBill::OnBillHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this)
    {
        bHitWallThisFrame = true;
        if (GetBulletBillAction() == EWFBulletBillAction::Launch && ActionTimer > 70)
        {
            OnBulletBillImpact(OtherActor);
        }
    }
}

void AWFBulletBill::OnBillOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && GetBulletBillAction() == EWFBulletBillAction::Launch)
    {
        OnBulletBillImpact(OtherActor);
        UGameplayStatics::ApplyDamage(
            OtherActor, static_cast<float>(DamageAmount), nullptr, this, UDamageType::StaticClass());
        OnBulletBillDamagePlayer(OtherActor, DamageAmount);
    }
}
