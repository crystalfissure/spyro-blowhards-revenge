#include "WFWhomp.h"

#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "SM64PowerStar.h"
#include "SM64Collectible.h"
#include "SM64CourseManager.h"

AWFWhomp::AWFWhomp()
{
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;
    CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionBox->SetNotifyRigidBodyCollision(true);
    CollisionBox->OnComponentHit.AddDynamic(this, &AWFWhomp::OnWhompHit);

    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(CollisionBox);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ExactCollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExactCollisionMesh"));
    ExactCollisionMesh->SetupAttachment(CollisionBox);
    ExactCollisionMesh->SetVisibility(false, true);
    ExactCollisionMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ExactCollisionMesh->SetNotifyRigidBodyCollision(true);
    ExactCollisionMesh->OnComponentHit.AddDynamic(this, &AWFWhomp::OnWhompHit);
}

void AWFWhomp::BeginPlay()
{
    HomeLocation = GetActorLocation();
    HomeRotation = GetActorRotation();
    bHomeCaptured = true;
    Super::BeginPlay();
}

void AWFWhomp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CollisionBox->SetBoxExtent(CollisionExtent);
    if (DefaultSkeletalMesh)
    {
        CharacterMesh->SetSkeletalMesh(DefaultSkeletalMesh);
    }
    ExactCollisionMesh->SetStaticMesh(DefaultCollisionMesh);
    ExactCollisionMesh->SetCollisionEnabled(DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    CollisionBox->SetCollisionEnabled(!DefaultCollisionMesh && bAllowProxyCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AWFWhomp::OnWhompHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor->IsA<APawn>()
        && (GetWhompAction() == EWFWhompAction::Jump || GetWhompAction() == EWFWhompAction::Land
            || GetWhompAction() == EWFWhompAction::Downed))
    {
        OnWhompPlayerContact(OtherActor, ContactDamage, true);
    }
}

void AWFWhomp::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation();
        HomeRotation = GetActorRotation();
        bHomeCaptured = true;
    }

    SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
    CharacterMesh->SetRelativeRotation(FRotator::ZeroRotator);
    CharacterMesh->SetVisibility(bActEnabled, true);
    ExactCollisionMesh->SetCollisionEnabled(bActEnabled && DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    CollisionBox->SetCollisionEnabled(bActEnabled && !DefaultCollisionMesh && bAllowProxyCollisionFallback
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Health = bKingWhomp ? 3 : 1;
    FacePitchDegrees = 0.0f;
    VerticalVelocityPerFrame = 0.0f;
    PitchVelocityDegreesPerFrame = 0.0f;
    bBossIntroRequested = false;
    bBossDefeatDialogRequested = false;
    bPreparationComplete = false;
    bLanded = false;
    bRecovering = false;
    bPendingHeadbash = false;
    PendingAttackInstigator = nullptr;
    ShakeFrame = 0;
    LandedFrames = 0;
    SetActorTickEnabled(bActEnabled);
    ActionCode = static_cast<int32>(EWFWhompAction::Initialize);
    ActionTimer = 0;
    if (WalkAnimation)
    {
        CharacterMesh->PlayAnimation(WalkAnimation, true);
        CharacterMesh->GlobalAnimRateScale = 1.0f;
    }
    OnRequestWhompAnimation(EWFWhompAction::Initialize, 1.0f);
}

EWFWhompAction AWFWhomp::GetWhompAction() const
{
    return static_cast<EWFWhompAction>(ActionCode);
}

void AWFWhomp::EnterWhompAction(EWFWhompAction NewAction, float AnimationRate)
{
    SetActionCode(static_cast<int32>(NewAction));
    UAnimSequence* NativeAnimation = NewAction == EWFWhompAction::PrepareJump
        ? PrepareJumpAnimation : WalkAnimation;
    if (NativeAnimation)
    {
        CharacterMesh->PlayAnimation(NativeAnimation, NewAction != EWFWhompAction::PrepareJump);
        CharacterMesh->GlobalAnimRateScale = AnimationRate;
    }
    OnRequestWhompAnimation(NewAction, AnimationRate);
    if (NewAction == EWFWhompAction::PrepareJump)
    {
        bPreparationComplete = false;
    }
}

void AWFWhomp::MoveForwardPerFrame(float Distance)
{
    AddActorWorldOffset(GetSM64ForwardVector() * Distance, true);
}

void AWFWhomp::TurnTowardPlayer(float MaxDegreesPerFrame)
{
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        return;
    }
    TurnSM64YawTowardLocation(Player->GetActorLocation(), MaxDegreesPerFrame);
}

bool AWFWhomp::PlayerWithinForwardCone(float Distance, float HalfAngleDegrees) const
{
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        return false;
    }
    FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    ToPlayer.Z = 0.0f;
    const float PlayerDistance = ToPlayer.Size();
    if (PlayerDistance >= Distance || PlayerDistance <= SMALL_NUMBER)
    {
        return false;
    }
    ToPlayer /= PlayerDistance;
    return FVector::DotProduct(GetSM64ForwardVector(), ToPlayer)
        > FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

bool AWFWhomp::ApplyVerticalMovementAndCheckFloor()
{
    VerticalVelocityPerFrame = FMath::Max(-78.0f, VerticalVelocityPerFrame - 4.0f);
    FVector NextLocation = GetActorLocation();
    NextLocation.Z += VerticalVelocityPerFrame;

    FHitResult FloorHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WFWhompFloor), false, this);
    const FVector TraceStart(NextLocation.X, NextLocation.Y, NextLocation.Z + 50.0f);
    const FVector TraceEnd(NextLocation.X, NextLocation.Y, NextLocation.Z - 250.0f);
    const bool bHasFloor = GetWorld() && GetWorld()->LineTraceSingleByChannel(
        FloorHit, TraceStart, TraceEnd, FloorTraceChannel, QueryParams);

    if (bHasFloor && VerticalVelocityPerFrame <= 0.0f && NextLocation.Z <= FloorHit.ImpactPoint.Z)
    {
        NextLocation.Z = FloorHit.ImpactPoint.Z;
        VerticalVelocityPerFrame = 0.0f;
        SetActorLocation(NextLocation, true);
        return true;
    }

    SetActorLocation(NextLocation, true);
    return false;
}

void AWFWhomp::StartRecovery()
{
    bRecovering = true;
    PitchVelocityDegreesPerFrame = -2.8125f; // -0x200 angle units/frame
}

void AWFWhomp::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const float PlayerDistance = Player ? FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) : BIG_NUMBER;

    switch (GetWhompAction())
    {
        case EWFWhompAction::Initialize:
            if (bKingWhomp)
            {
                if (!bBossIntroRequested && PlayerDistance < BossActivationDistance)
                {
                    bBossIntroRequested = true;
                    OnRequestBossIntroDialog(114);
                    if (bAutoCompleteBossDialogs)
                    {
                        CompleteBossIntroDialog();
                    }
                }
            }
            else if (PlayerDistance < SmallActivationDistance)
            {
                EnterWhompAction(EWFWhompAction::Patrol, 1.0f);
            }
            break;

        case EWFWhompAction::Patrol:
        {
            float Speed = 3.0f;
            if (FVector::Dist2D(GetActorLocation(), HomeLocation) > PatrolDistance)
            {
                EnterWhompAction(EWFWhompAction::Turn, 1.0f);
                break;
            }
            if (PlayerWithinForwardCone(NoticeDistance, 45.0f))
            {
                Speed = 9.0f;
                if (PlayerDistance < AttackDistance)
                {
                    EnterWhompAction(EWFWhompAction::PrepareJump, 1.0f);
                    break;
                }
            }
            MoveForwardPerFrame(Speed);
            break;
        }

        case EWFWhompAction::BossChase:
            MoveForwardPerFrame(PlayerDistance < NoticeDistance ? 9.0f : 3.0f);
            TurnTowardPlayer(2.8125f); // 0x200 angle units/frame
            if (ActionTimer > 30 && PlayerWithinForwardCone(NoticeDistance, 45.0f)
                && PlayerDistance < AttackDistance)
            {
                EnterWhompAction(EWFWhompAction::PrepareJump, 1.0f);
            }
            if (Player && Player->GetActorLocation().Z < GetActorLocation().Z - 1000.0f)
            {
                SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
                EnterWhompAction(EWFWhompAction::Initialize, 1.0f);
                OnBossMusicRequested(false);
            }
            break;

        case EWFWhompAction::PrepareJump:
            if (bPreparationComplete || ActionTimer >= PrepareJumpFallbackFrames)
            {
                EnterWhompAction(EWFWhompAction::Jump, 1.0f);
            }
            break;

        case EWFWhompAction::Jump:
            if (ActionTimer == 0)
            {
                VerticalVelocityPerFrame = 40.0f;
                PitchVelocityDegreesPerFrame = 0.0f;
            }
            if (ActionTimer >= 8)
            {
                PitchVelocityDegreesPerFrame += 1.40625f; // +0x100 angle units/frame
                FacePitchDegrees += PitchVelocityDegreesPerFrame;
                if (FacePitchDegrees >= 90.0f)
                {
                    FacePitchDegrees = 90.0f;
                    PitchVelocityDegreesPerFrame = 0.0f;
                    EnterWhompAction(EWFWhompAction::Land, 1.0f);
                }
                CharacterMesh->SetRelativeRotation(FRotator(FacePitchDegrees, 0.0f, 0.0f));
            }
            ApplyVerticalMovementAndCheckFloor();
            break;

        case EWFWhompAction::Land:
            if (!bLanded && ApplyVerticalMovementAndCheckFloor())
            {
                bLanded = true;
                LandedFrames = 0;
                OnWhompBodySlam();
            }
            else if (bLanded)
            {
                ++LandedFrames;
                if (LandedFrames >= 1)
                {
                    EnterWhompAction(EWFWhompAction::Downed, 1.0f);
                    bLanded = false;
                }
            }
            break;

        case EWFWhompAction::Downed:
            if (bPendingHeadbash && !bRecovering)
            {
                bPendingHeadbash = false;
                --Health;
                OnWhompDamaged(Health, PendingAttackInstigator);
                if (Health <= 0)
                {
                    if (!bKingWhomp)
                    {
                        OnDropYellowCoins(5);
                        SpawnYellowCoinDrops(5);
                    }
                    EnterWhompAction(EWFWhompAction::Die, 1.0f);
                    break;
                }
                ShakeFrame = 0;
            }

            if (!bRecovering && bKingWhomp && Health < 3 && ShakeFrame < 10)
            {
                AddActorWorldOffset(FVector(0.0f, 0.0f, (ShakeFrame & 1) ? 8.0f : -8.0f));
                ++ShakeFrame;
                if (ShakeFrame >= 10)
                {
                    StartRecovery();
                }
            }
            else if (!bRecovering && ActionTimer > 100)
            {
                StartRecovery();
            }

            if (bRecovering)
            {
                FacePitchDegrees = FMath::Max(0.0f, FacePitchDegrees + PitchVelocityDegreesPerFrame);
                CharacterMesh->SetRelativeRotation(FRotator(FacePitchDegrees, 0.0f, 0.0f));
                if (FacePitchDegrees <= 0.0f)
                {
                    bRecovering = false;
                    EnterWhompAction(bKingWhomp ? EWFWhompAction::BossChase : EWFWhompAction::Patrol, 1.0f);
                }
            }
            break;

        case EWFWhompAction::Turn:
            if (ActionTimer <= 31)
            {
                AddActorWorldRotation(FRotator(0.0f, -5.625f, 0.0f)); // source +0x400, stored UE yaw is negated
            }
            else
            {
                MoveForwardPerFrame(3.0f);
                if (ActionTimer > 42)
                {
                    EnterWhompAction(EWFWhompAction::Patrol, 1.0f);
                }
            }
            break;

        case EWFWhompAction::Die:
            if (bKingWhomp)
            {
                if (!bBossDefeatDialogRequested)
                {
                    bBossDefeatDialogRequested = true;
                    OnRequestBossDefeatDialog(115);
                    if (bAutoCompleteBossDialogs)
                    {
                        CompleteBossDefeatDialog();
                    }
                }
            }
            else
            {
                CharacterMesh->SetVisibility(false, true);
                CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                OnWhompDefeated(false);
                SetActorTickEnabled(false);
            }
            break;

        case EWFWhompAction::StopBossMusic:
            if (ActionTimer == 60)
            {
                OnBossMusicRequested(false);
            }
            break;
    }

    FinishActionFrame(PreviousAction);
}

void AWFWhomp::SpawnYellowCoinDrops(int32 Count)
{
    if (!GetWorld() || !DropCoinClass || Count <= 0)
    {
        return;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Count);
        const FVector Offset(FMath::Cos(Angle) * 120.0f, FMath::Sin(Angle) * 120.0f, 90.0f);
        ASM64Collectible* Coin = GetWorld()->SpawnActor<ASM64Collectible>(
            DropCoinClass, GetActorLocation() + Offset, FRotator::ZeroRotator);
        if (Coin)
        {
            Coin->StableId = FName(*(StableId.ToString() + FString::Printf(TEXT("_Drop_%02d"), Index)));
            Coin->CoinValue = 1;
            Coin->bDestroyOnActReset = true;
            Coin->ActMask = ActMask;
            if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
            {
                Coin->SetCurrentAct(Manager->CurrentAct);
            }
        }
    }
}

void AWFWhomp::CompleteBossIntroDialog()
{
    if (bKingWhomp && GetWhompAction() == EWFWhompAction::Initialize)
    {
        EnterWhompAction(EWFWhompAction::BossChase, 1.0f);
        OnBossMusicRequested(true);
    }
}

void AWFWhomp::CompleteBossDefeatDialog()
{
    if (!bKingWhomp || GetWhompAction() != EWFWhompAction::Die)
    {
        return;
    }
    CharacterMesh->SetVisibility(false, true);
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OnWhompDefeated(true);
    OnSpawnBossStar(BossStarIndex, BossStarLocation);
    for (TActorIterator<ASM64PowerStar> It(GetWorld()); It; ++It)
    {
        ASM64PowerStar* Star = *It;
        if (Star && Star->StarIndex == BossStarIndex && !Star->b100CoinStar)
        {
            Star->BeginSpawnSequence(GetActorLocation(), BossStarLocation, false);
            break;
        }
    }
    EnterWhompAction(EWFWhompAction::StopBossMusic, 1.0f);
}

void AWFWhomp::NotifyPreparationAnimationComplete()
{
    bPreparationComplete = true;
}

void AWFWhomp::NotifyHeadbashOnBack(AActor* InstigatorActor)
{
    if (GetWhompAction() == EWFWhompAction::Downed && !bPendingHeadbash)
    {
        bPendingHeadbash = true;
        PendingAttackInstigator = InstigatorActor;
    }
}

bool AWFWhomp::HandleSM64Attack_Implementation(ESM64AttackType AttackType, AActor* InstigatorActor,
    FVector ImpactPoint, FVector ImpactDirection)
{
    if (AttackType != ESM64AttackType::Headbash || GetWhompAction() != EWFWhompAction::Downed)
    {
        return false;
    }
    NotifyHeadbashOnBack(InstigatorActor);
    return true;
}
