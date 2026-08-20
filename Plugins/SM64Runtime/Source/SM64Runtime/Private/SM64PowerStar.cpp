#include "SM64PowerStar.h"

#include "GameFramework/Pawn.h"
#include "SM64CourseManager.h"

ASM64PowerStar::ASM64PowerStar()
{
    Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
    RootComponent = Trigger;
    Trigger->SetSphereRadius(80.0f);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64PowerStar::OnStarOverlap);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Trigger);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASM64PowerStar::BeginPlay()
{
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation();
        bHomeCaptured = true;
    }
    Super::BeginPlay();
}

void ASM64PowerStar::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultMesh)
    {
        Mesh->SetStaticMesh(DefaultMesh);
    }
}

void ASM64PowerStar::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation();
        bHomeCaptured = true;
    }
    Collector = nullptr;
    bSpawnSequenceActive = false;
    bNoExit = bNoExit || b100CoinStar;
    bool bAlreadyCollected = false;
    if (const ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        bAlreadyCollected = b100CoinStar
            ? Manager->Progress.bCollected100CoinStar
            : Manager->Progress.HasMissionStar(StarIndex);
    }
    const bool bShowCollected = bAlreadyCollected && bStartAvailable && bShowCollectedStaticStars;
    ActionCode = static_cast<int32>(bShowCollected
        ? ESM64PowerStarState::Collected
        : (bStartAvailable ? ESM64PowerStarState::Available : ESM64PowerStarState::Hidden));
    ActionTimer = 0;
    SetActorLocation(HomeLocation);
    SetStarCollectible(bStartAvailable || bShowCollected, bStartAvailable && !bAlreadyCollected);
    ApplyCollectedPresentation(bShowCollected);
}

ESM64PowerStarState ASM64PowerStar::GetPowerStarState() const
{
    return static_cast<ESM64PowerStarState>(ActionCode);
}

void ASM64PowerStar::EnterStarState(ESM64PowerStarState NewState)
{
    SetActionCode(static_cast<int32>(NewState));
}

void ASM64PowerStar::SetStarCollectible(bool bVisible, bool bCollectible)
{
    Mesh->SetVisibility(bVisible && bActEnabled, true);
    Trigger->SetCollisionEnabled(bCollectible && bActEnabled
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ASM64PowerStar::ApplyCollectedPresentation(bool bCollected)
{
    bCollectedPresentation = bCollected;
    Mesh->SetScalarParameterValueOnMaterials(OpacityParameter, bCollected ? CollectedOpacity : 1.0f);
    Mesh->SetVectorParameterValueOnMaterials(
        TintParameter, bCollected ? FVector(0.55f, 0.75f, 1.0f) : FVector::OneVector);
    Mesh->SetRenderCustomDepth(bCollected);
}

void ASM64PowerStar::BeginSpawnSequence(FVector SpawnLocation, FVector TargetHomeLocation,
    bool bRedCoinStyleCutscene)
{
    SpawnStartLocation = SpawnLocation;
    HomeLocation = TargetHomeLocation;
    bHomeCaptured = true;
    bRedCoinSpawnStyle = bRedCoinStyleCutscene;
    bSpawnSequenceActive = true;
    ApplyCollectedPresentation(false);
    SetActorLocation(SpawnStartLocation);
    SetStarCollectible(true, false);
    EnterStarState(ESM64PowerStarState::SpawnPause);
    OnStarSpawnCutsceneStarted(bRedCoinStyleCutscene);
}

void ASM64PowerStar::RevealImmediately(FVector WorldLocation)
{
    HomeLocation = WorldLocation;
    bHomeCaptured = true;
    ApplyCollectedPresentation(false);
    SetActorLocation(WorldLocation);
    EnterStarState(ESM64PowerStarState::Available);
    SetStarCollectible(true, true);
    OnStarBecameCollectible();
}

void ASM64PowerStar::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    switch (GetPowerStarState())
    {
        case ESM64PowerStarState::Hidden:
            break;

        case ESM64PowerStarState::SpawnPause:
            Mesh->AddLocalRotation(FRotator(0.0f, -22.5f, 0.0f)); // source +0x1000, stored yaw negated
            if (ActionTimer > 20)
            {
                LinearFlightBase = GetActorLocation();
                LinearVelocityPerFrame = (HomeLocation - LinearFlightBase) / 30.0f;
                EnterStarState(ESM64PowerStarState::FlyToHome);
            }
            break;

        case ESM64PowerStarState::FlyToHome:
        {
            LinearFlightBase += LinearVelocityPerFrame;
            FVector Location = LinearFlightBase;
            Location.Z += FMath::Sin(static_cast<float>(ActionTimer) * PI / 30.0f) * 400.0f;
            SetActorLocation(Location);
            Mesh->AddLocalRotation(FRotator(0.0f, -22.5f, 0.0f));
            OnStarSpawnSparkle(Location);
            if (ActionTimer == 30)
            {
                EnterStarState(ESM64PowerStarState::Settle);
            }
            break;
        }

        case ESM64PowerStarState::Settle:
        {
            FVector Location = GetActorLocation();
            Location.Z += ActionTimer < 20 ? static_cast<float>(20 - ActionTimer) : -10.0f;
            SetActorLocation(Location);
            OnStarSpawnSparkle(Location);
            if (Location.Z < HomeLocation.Z)
            {
                SetActorLocation(HomeLocation);
                SetStarCollectible(true, true);
                EnterStarState(ESM64PowerStarState::Available);
                OnStarBecameCollectible();
            }
            break;
        }

        case ESM64PowerStarState::Available:
            Mesh->AddLocalRotation(FRotator(0.0f, -11.25f, 0.0f)); // source +0x800
            if (bSpawnSequenceActive && ActionTimer == 20)
            {
                bSpawnSequenceActive = false;
                OnStarSpawnCutsceneFinished();
            }
            break;

        case ESM64PowerStarState::CollectionCutscene:
        case ESM64PowerStarState::Collected:
            break;
    }
    FinishActionFrame(PreviousAction);
}

void ASM64PowerStar::OnStarOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA<APawn>() || GetPowerStarState() != ESM64PowerStarState::Available)
    {
        return;
    }
    Collector = OtherActor;
    SetStarCollectible(false, false);
    EnterStarState(ESM64PowerStarState::CollectionCutscene);
    OnRequestStarCollectionCutscene(OtherActor, StarIndex, bNoExit || b100CoinStar);
    if (!bWaitForBlueprintCollectionCutscene)
    {
        CompleteCollectionCutscene();
    }
}

void ASM64PowerStar::CompleteCollectionCutscene()
{
    if (GetPowerStarState() != ESM64PowerStarState::CollectionCutscene)
    {
        return;
    }
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        Manager->CollectStar(StarIndex, b100CoinStar);
    }
    EnterStarState(ESM64PowerStarState::Collected);
    Collector = nullptr;
}
