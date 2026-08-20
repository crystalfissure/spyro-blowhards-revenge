#include "SM64BlueCoinChallenge.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "SM64CourseManager.h"

ASM64BlueCoinSwitch::ASM64BlueCoinSwitch()
{
    SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
    RootComponent = SwitchMesh;
    SwitchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ExactCollisionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExactCollisionMesh"));
    ExactCollisionMesh->SetupAttachment(SwitchMesh);
    ExactCollisionMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ExactCollisionMesh->SetVisibility(false, true);
    ExactCollisionMesh->SetHiddenInGame(true, true);
}

void ASM64BlueCoinSwitch::BeginPlay()
{
    InitialTransform = GetActorTransform();
    bTransformCaptured = true;
    Super::BeginPlay();
}

void ASM64BlueCoinSwitch::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        SwitchMesh->SetStaticMesh(DefaultMesh);
    }
    ExactCollisionMesh->SetStaticMesh(DefaultCollisionMesh);
    ExactCollisionMesh->SetCollisionEnabled(DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ASM64BlueCoinSwitch::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bTransformCaptured)
    {
        InitialTransform = GetActorTransform();
        bTransformCaptured = true;
    }
    SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
    ActionCode = static_cast<int32>(ESM64BlueCoinSwitchState::Idle);
    ActionTimer = 0;
    SwitchMesh->SetVisibility(bActEnabled, true);
    SwitchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExactCollisionMesh->SetCollisionEnabled(bActEnabled && DefaultCollisionMesh
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

ESM64BlueCoinSwitchState ASM64BlueCoinSwitch::GetSwitchState() const
{
    return static_cast<ESM64BlueCoinSwitchState>(ActionCode);
}

void ASM64BlueCoinSwitch::ActivateChallengeCoins(AActor* InstigatorActor)
{
    for (TActorIterator<ASM64TimedBlueCoin> It(GetWorld()); It; ++It)
    {
        if (It->ChallengeId == ChallengeId)
        {
            It->ActivateFromSwitch(this);
        }
    }
}

bool ASM64BlueCoinSwitch::HasPendingChallengeCoins() const
{
    if (!GetWorld())
    {
        return false;
    }
    for (TActorIterator<ASM64TimedBlueCoin> It(GetWorld()); It; ++It)
    {
        if (It->ChallengeId == ChallengeId && It->IsPendingForSwitch())
        {
            return true;
        }
    }
    return false;
}

bool ASM64BlueCoinSwitch::PressSwitch(AActor* InstigatorActor)
{
    if (GetSwitchState() != ESM64BlueCoinSwitchState::Idle)
    {
        return false;
    }
    OnBlueCoinSwitchPressed(InstigatorActor);
    SetActionCode(static_cast<int32>(ESM64BlueCoinSwitchState::Receding));
    return true;
}

bool ASM64BlueCoinSwitch::HandleSM64Attack_Implementation(ESM64AttackType AttackType,
    AActor* InstigatorActor, FVector ImpactPoint, FVector ImpactDirection)
{
    return AttackType == RequiredAttack && PressSwitch(InstigatorActor);
}

void ASM64BlueCoinSwitch::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    switch (GetSwitchState())
    {
        case ESM64BlueCoinSwitchState::Idle:
            break;

        case ESM64BlueCoinSwitchState::Receding:
            if (ActionTimer > 5)
            {
                SwitchMesh->SetVisibility(false, true);
                SwitchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ActivateChallengeCoins(this);
                SetActionCode(static_cast<int32>(ESM64BlueCoinSwitchState::Ticking));
            }
            else
            {
                AddActorWorldOffset(FVector(0.0f, 0.0f, -20.0f), true);
            }
            break;

        case ESM64BlueCoinSwitchState::Ticking:
            OnBlueCoinSwitchTick(ActionTimer < 200, FMath::Max(0, 240 - ActionTimer));
            if (!HasPendingChallengeCoins() || ActionTimer > 240)
            {
                OnBlueCoinSwitchExpired();
                SetActionCode(static_cast<int32>(ESM64BlueCoinSwitchState::Expired));
            }
            break;

        case ESM64BlueCoinSwitchState::Expired:
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            break;
    }
    FinishActionFrame(PreviousAction);
}

ASM64TimedBlueCoin::ASM64TimedBlueCoin()
{
    Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
    RootComponent = Trigger;
    Trigger->SetSphereRadius(80.0f);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64TimedBlueCoin::OnCoinOverlap);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Trigger);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASM64TimedBlueCoin::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
}

void ASM64TimedBlueCoin::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    ActionCode = static_cast<int32>(ESM64TimedBlueCoinState::Waiting);
    ActionTimer = 0;
    SetCoinPresentation(false, false);
}

ESM64TimedBlueCoinState ASM64TimedBlueCoin::GetCoinState() const
{
    return static_cast<ESM64TimedBlueCoinState>(ActionCode);
}

bool ASM64TimedBlueCoin::IsPendingForSwitch() const
{
    return GetCoinState() == ESM64TimedBlueCoinState::Active;
}

void ASM64TimedBlueCoin::SetCoinPresentation(bool bVisible, bool bCollectible)
{
    Mesh->SetVisibility(bVisible && bActEnabled, true);
    Trigger->SetCollisionEnabled(bCollectible && bActEnabled
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ASM64TimedBlueCoin::ActivateFromSwitch(AActor* SwitchActor)
{
    if (GetCoinState() != ESM64TimedBlueCoinState::Waiting)
    {
        return;
    }
    SetActionCode(static_cast<int32>(ESM64TimedBlueCoinState::Active));
    SetCoinPresentation(true, true);
    OnTimedBlueCoinActivated();
}

void ASM64TimedBlueCoin::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    if (GetCoinState() == ESM64TimedBlueCoinState::Active)
    {
        Mesh->AddLocalRotation(FRotator(0.0f, SpinDegreesPerFrame, 0.0f));
        if (ActionTimer >= 200)
        {
            const int32 TimeBlinking = ActionTimer - 200;
            const bool bInvisibleBlink = (TimeBlinking & 1) != 0;
            SetCoinPresentation(!bInvisibleBlink, true);
            if (bInvisibleBlink && TimeBlinking / 2 > 20)
            {
                SetCoinPresentation(false, false);
                OnTimedBlueCoinExpired();
                SetActionCode(static_cast<int32>(ESM64TimedBlueCoinState::Expired));
            }
        }
    }
    FinishActionFrame(PreviousAction);
}

void ASM64TimedBlueCoin::OnCoinOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA<APawn>() || GetCoinState() != ESM64TimedBlueCoinState::Active)
    {
        return;
    }
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        Manager->AddCoin(CoinValue, false);
    }
    SetCoinPresentation(false, false);
    SetActionCode(static_cast<int32>(ESM64TimedBlueCoinState::Collected));
    OnTimedBlueCoinCollected(OtherActor);
}
