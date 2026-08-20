#include "WFButterfly.h"

#include "Animation/AnimSequence.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SM64Collectible.h"
#include "SM64CourseManager.h"

AWFButterfly::AWFButterfly()
{
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(SceneRoot);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWFButterfly::BeginPlay()
{
    HomeLocation = GetActorLocation();
    HomeRotation = GetActorRotation();
    bHomeCaptured = true;
    Super::BeginPlay();
}

void AWFButterfly::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultSkeletalMesh)
    {
        CharacterMesh->SetSkeletalMesh(DefaultSkeletalMesh);
    }
}

void AWFButterfly::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    if (!bHomeCaptured)
    {
        HomeLocation = GetActorLocation();
        HomeRotation = GetActorRotation();
        bHomeCaptured = true;
    }
    SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
    ButterflyState = bTripletButterfly ? EWFButterflyState::TripletWander : EWFButterflyState::Resting;
    ActionTimer = 0;
    VerticalPhase = static_cast<float>(GetTypeHash(StableId) % 101);
    TripletSpeed = 30.0f;
    bSpawnedOneUp = false;
    CharacterMesh->SetVisibility(bActEnabled, true);
    if (bTripletButterfly ? FlightAnimation : RestAnimation)
    {
        CharacterMesh->PlayAnimation(bTripletButterfly ? FlightAnimation : RestAnimation, true);
    }
}

void AWFButterfly::EnterState(EWFButterflyState NewState)
{
    ButterflyState = NewState;
    ActionTimer = 0;
    if (NewState == EWFButterflyState::Resting && RestAnimation)
    {
        CharacterMesh->PlayAnimation(RestAnimation, true);
    }
    else if (FlightAnimation)
    {
        CharacterMesh->PlayAnimation(FlightAnimation, true);
    }
}

void AWFButterfly::MoveToward(const FVector& Target, float SpeedPerFrame, float MaxYawDegrees,
    float MaxPitchDegrees, bool bApplyWingBob)
{
    const FVector Delta = Target - GetActorLocation();
    const float Horizontal = FVector2D(Delta.X, Delta.Y).Size();
    const FRotator Desired = Delta.Rotation();
    FRotator Current = GetActorRotation();
    Current.Yaw = FMath::FixedTurn(Current.Yaw, Desired.Yaw, MaxYawDegrees);
    Current.Pitch = FMath::FixedTurn(Current.Pitch, Desired.Pitch, MaxPitchDegrees);
    SetActorRotation(Current);
    FVector Movement = Current.Vector() * SpeedPerFrame;
    if (bApplyWingBob)
    {
        Movement.Z += FMath::Cos(VerticalPhase * 2.0f * PI / 100.0f) * 5.0f;
    }
    SetActorLocation(GetActorLocation() + Movement, true);
    VerticalPhase = FMath::Fmod(VerticalPhase + 1.0f, 101.0f);
}

void AWFButterfly::SpawnSelectedOneUp()
{
    if (bSpawnedOneUp || !bSelectedForOneUp || !OneUpClass || !GetWorld())
    {
        return;
    }
    ASM64Collectible* OneUp = GetWorld()->SpawnActor<ASM64Collectible>(
        OneUpClass, GetActorLocation(), FRotator::ZeroRotator);
    if (OneUp)
    {
        OneUp->StableId = FName(*(StableId.ToString() + TEXT("_OneUp")));
        OneUp->bOneUp = true;
        OneUp->CoinValue = 0;
        OneUp->bDestroyOnActReset = true;
        OneUp->ActMask = ActMask;
        if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
        {
            OneUp->SetCurrentAct(Manager->CurrentAct);
        }
    }
    bSpawnedOneUp = true;
}

void AWFButterfly::SimulateSM64Frame_Implementation()
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player)
    {
        ++ActionTimer;
        return;
    }

    switch (ButterflyState)
    {
        case EWFButterflyState::Resting:
            if (FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(1000.0f))
            {
                SetActorRotation(FRotator(0.0f, Player->GetActorRotation().Yaw, 0.0f));
                EnterState(EWFButterflyState::FollowPlayer);
            }
            break;

        case EWFButterflyState::FollowPlayer:
        {
            const FVector PhaseOffset(VerticalPhase * 1.25f, VerticalPhase * 1.25f,
                (VerticalPhase * 5.0f + 256.0f) * 0.25f);
            MoveToward(Player->GetActorLocation() + PhaseOffset, 7.0f, 4.21875f, 7.03125f, true);
            if (FVector::DistSquared(Player->GetActorLocation(), HomeLocation) > FMath::Square(1200.0f))
            {
                EnterState(EWFButterflyState::ReturnHome);
            }
            break;
        }

        case EWFButterflyState::ReturnHome:
            MoveToward(HomeLocation, 7.0f, 11.25f, 0.439453125f, false);
            if (FVector::DistSquared(GetActorLocation(), HomeLocation) < 144.0f)
            {
                SetActorLocation(HomeLocation);
                EnterState(EWFButterflyState::Resting);
            }
            break;

        case EWFButterflyState::TripletWander:
        {
            if (FVector::DistSquared(Player->GetActorLocation(), HomeLocation) > FMath::Square(1500.0f))
            {
                CharacterMesh->SetVisibility(false, true);
                EnterState(EWFButterflyState::Activated);
                break;
            }
            TripletSpeed = FMath::Max(8.0f, TripletSpeed - 0.5f);
            const float Radians = FMath::DegreesToRadians(BaseYawDegrees + ActionTimer * 2.0f);
            FVector Target = HomeLocation + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f) * 180.0f;
            Target.Z += 75.0f + FMath::Sin(VerticalPhase * 2.0f * PI / 100.0f) * 50.0f;
            MoveToward(Target, TripletSpeed, 7.03125f, 3.515625f, false);
            if (bSelectedForOneUp && ActionTimer > 110
                && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(200.0f))
            {
                SpawnSelectedOneUp();
                CharacterMesh->SetVisibility(false, true);
                EnterState(EWFButterflyState::Activated);
            }
            break;
        }

        case EWFButterflyState::Activated:
            break;
    }
    ++ActionTimer;
}
