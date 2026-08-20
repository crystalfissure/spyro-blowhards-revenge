#include "WFPiranhaPlant.h"

#include "Animation/AnimSequence.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64Collectible.h"
#include "SM64CourseManager.h"
#include "GameFramework/DamageType.h"

AWFPiranhaPlant::AWFPiranhaPlant()
{
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    RootComponent = InteractionSphere;
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWFPiranhaPlant::OnPlantOverlap);

    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(InteractionSphere);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWFPiranhaPlant::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultSkeletalMesh)
    {
        CharacterMesh->SetSkeletalMesh(DefaultSkeletalMesh);
    }
}

void AWFPiranhaPlant::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    ActionCode = static_cast<int32>(EWFPiranhaAction::Idle);
    ActionTimer = 0;
    PlantScale = 1.0f;
    CharacterMesh->SetRelativeScale3D(FVector::OneVector);
    CharacterMesh->SetVisibility(bActEnabled, true);
    InteractionSphere->SetCollisionEnabled(bActEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    bAnimationComplete = false;
    bLullabyPlaying = false;
    LastAttackInstigator = nullptr;
    PlayNativeAnimation(EWFPiranhaAction::Idle);
    OnRequestPiranhaAnimation(EWFPiranhaAction::Idle);
}

EWFPiranhaAction AWFPiranhaPlant::GetPiranhaAction() const
{
    return static_cast<EWFPiranhaAction>(ActionCode);
}

void AWFPiranhaPlant::EnterPiranhaAction(EWFPiranhaAction NewAction)
{
    SetActionCode(static_cast<int32>(NewAction));
    bAnimationComplete = false;
    PlayNativeAnimation(NewAction);
    OnRequestPiranhaAnimation(NewAction);
}

void AWFPiranhaPlant::PlayNativeAnimation(EWFPiranhaAction NewAction)
{
    int32 AnimationIndex = 8; // sleeping/idle/respawn
    bool bLoop = true;
    if (NewAction == EWFPiranhaAction::Biting)
    {
        AnimationIndex = 0;
    }
    else if (NewAction == EWFPiranhaAction::StoppedBiting)
    {
        AnimationIndex = 6;
        bLoop = false;
    }
    else if (NewAction == EWFPiranhaAction::Attacked
        || NewAction == EWFPiranhaAction::ShrinkAndDie)
    {
        AnimationIndex = 2;
        bLoop = false;
    }
    if (Animations.IsValidIndex(AnimationIndex) && Animations[AnimationIndex])
    {
        CharacterMesh->PlayAnimation(Animations[AnimationIndex], bLoop);
    }
}

bool AWFPiranhaPlant::IsPlayerMovingFast(const APawn* Player) const
{
    if (!Player)
    {
        return false;
    }
    const FVector VelocityPerFrame = Player->GetVelocity() / 30.0f;
    return VelocityPerFrame.Size2D() > FastPlayerSpeedPerFrame
        || FMath::Abs(VelocityPerFrame.Z) > FastPlayerSpeedPerFrame;
}

void AWFPiranhaPlant::TurnTowardPlayer(float MaxDegreesPerFrame)
{
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        return;
    }
    TurnSM64YawTowardLocation(Player->GetActorLocation(), MaxDegreesPerFrame);
}

void AWFPiranhaPlant::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const float PlayerDistance = Player ? FVector::Dist(Player->GetActorLocation(), GetActorLocation()) : BIG_NUMBER;

    if (Player)
    {
        CharacterMesh->SetVisibility(bActEnabled && Player->GetActorLocation().Z <= WFHideAboveHeight, true);
    }

    switch (GetPiranhaAction())
    {
        case EWFPiranhaAction::Idle:
            if (PlayerDistance < ActivationDistance)
            {
                EnterPiranhaAction(EWFPiranhaAction::Sleeping);
            }
            break;

        case EWFPiranhaAction::Sleeping:
            if (PlayerDistance < WakeDistance && IsPlayerMovingFast(Player))
            {
                if (bLullabyPlaying)
                {
                    bLullabyPlaying = false;
                    OnLullabyStateChanged(false);
                }
                EnterPiranhaAction(EWFPiranhaAction::WokenUp);
            }
            else if (PlayerDistance < LullabyDistance && !bLullabyPlaying)
            {
                bLullabyPlaying = true;
                OnLullabyStateChanged(true);
            }
            else if (PlayerDistance >= LullabyDistance && bLullabyPlaying)
            {
                bLullabyPlaying = false;
                OnLullabyStateChanged(false);
            }
            break;

        case EWFPiranhaAction::WokenUp:
            if (ActionTimer == 0 && bLullabyPlaying)
            {
                bLullabyPlaying = false;
                OnLullabyStateChanged(false);
            }
            if (ActionTimer > 10)
            {
                EnterPiranhaAction(EWFPiranhaAction::Biting);
            }
            break;

        case EWFPiranhaAction::Biting:
            TurnTowardPlayer(5.625f); // 0x400 angle units/frame
            if (PlayerDistance > StopBitingDistance && (bAnimationComplete || ActionTimer > 70))
            {
                EnterPiranhaAction(EWFPiranhaAction::StoppedBiting);
            }
            break;

        case EWFPiranhaAction::StoppedBiting:
            if (PlayerDistance < WakeDistance && IsPlayerMovingFast(Player))
            {
                EnterPiranhaAction(EWFPiranhaAction::Biting);
            }
            else if (bAnimationComplete || ActionTimer > 20)
            {
                EnterPiranhaAction(EWFPiranhaAction::Sleeping);
            }
            break;

        case EWFPiranhaAction::Attacked:
            if (bAnimationComplete || ActionTimer > 35)
            {
                EnterPiranhaAction(EWFPiranhaAction::ShrinkAndDie);
            }
            break;

        case EWFPiranhaAction::ShrinkAndDie:
            if (ActionTimer == 0)
            {
                PlantScale = 1.0f;
            }
            if (PlantScale > 0.0f)
            {
                PlantScale -= 0.04f;
            }
            else
            {
                PlantScale = 0.0f;
                OnSpawnBlueCoin(FName(*(StableId.ToString() + TEXT("_BlueCoin"))), 5);
                SpawnBlueCoinDrop();
                EnterPiranhaAction(EWFPiranhaAction::WaitToRespawn);
            }
            CharacterMesh->SetRelativeScale3D(FVector(FMath::Max(0.0f, PlantScale)));
            break;

        case EWFPiranhaAction::WaitToRespawn:
            if (PlayerDistance > ActivationDistance)
            {
                EnterPiranhaAction(EWFPiranhaAction::Respawn);
            }
            break;

        case EWFPiranhaAction::Respawn:
            if (ActionTimer == 0)
            {
                PlantScale = 0.3f;
            }
            if (PlantScale < 1.0f)
            {
                PlantScale += 0.02f;
            }
            else
            {
                PlantScale = 1.0f;
                EnterPiranhaAction(EWFPiranhaAction::Idle);
            }
            CharacterMesh->SetRelativeScale3D(FVector(FMath::Min(1.0f, PlantScale)));
            break;
    }

    FinishActionFrame(PreviousAction);
}

void AWFPiranhaPlant::SpawnBlueCoinDrop()
{
    if (!GetWorld() || !BlueCoinClass)
    {
        return;
    }
    ASM64Collectible* Coin = GetWorld()->SpawnActor<ASM64Collectible>(
        BlueCoinClass, GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    if (Coin)
    {
        Coin->StableId = FName(*(StableId.ToString() + TEXT("_BlueCoin")));
        Coin->CoinValue = 5;
        Coin->bDestroyOnActReset = true;
        Coin->ActMask = ActMask;
        if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
        {
            Coin->SetCurrentAct(Manager->CurrentAct);
        }
    }
}

void AWFPiranhaPlant::NotifyCurrentAnimationComplete()
{
    bAnimationComplete = true;
}

void AWFPiranhaPlant::WakeWithoutDamage(AActor* InstigatorActor)
{
    if (GetPiranhaAction() == EWFPiranhaAction::Sleeping)
    {
        LastAttackInstigator = InstigatorActor;
        EnterPiranhaAction(EWFPiranhaAction::WokenUp);
    }
}

bool AWFPiranhaPlant::HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
    FVector ImpactPoint, FVector ImpactDirection)
{
    const EWFPiranhaAction CurrentAction = GetPiranhaAction();
    if (CurrentAction == EWFPiranhaAction::Attacked || CurrentAction == EWFPiranhaAction::ShrinkAndDie
        || CurrentAction == EWFPiranhaAction::WaitToRespawn || CurrentAction == EWFPiranhaAction::Respawn)
    {
        return false;
    }
    LastAttackInstigator = InstigatorActor;
    OnPiranhaAttacked(InstigatorActor);
    EnterPiranhaAction(EWFPiranhaAction::Attacked);
    return true;
}

void AWFPiranhaPlant::OnPlantOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && GetPiranhaAction() == EWFPiranhaAction::Biting)
    {
        UGameplayStatics::ApplyDamage(OtherActor, 3.0f, nullptr, this, UDamageType::StaticClass());
        OnPiranhaBite(OtherActor, 3);
    }
}
