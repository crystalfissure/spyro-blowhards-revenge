#include "SM64HiddenOneUp.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ASM64HiddenOneUp::ASM64HiddenOneUp()
{
    PrimaryActorTick.bCanEverTick = true;
    bOneUp = true;
    CoinValue = 0;
    Trigger->SetSphereRadius(30.0f);
}

void ASM64HiddenOneUp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Trigger->SetSphereRadius(30.0f);
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        HomeLocation = GetActorLocation();
    }
}

void ASM64HiddenOneUp::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    TouchedTriggerIndices.Reset();
    TouchedTriggerCount = 0;
    AccumulatedSeconds = 0.0f;
    ActionFrame = 0;
    MoveAnglePitchUnits = -0x4000;
    bRevealed = false;
    bTangible = false;
    bRevealOffsetApplied = false;
    SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorHiddenInGame(true);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASM64HiddenOneUp::RegisterHiddenTrigger(int32 TriggerIndex)
{
    if (!bActEnabled || bRevealed || TouchedTriggerIndices.Contains(TriggerIndex))
    {
        return;
    }
    TouchedTriggerIndices.Add(TriggerIndex);
    TouchedTriggerCount = TouchedTriggerIndices.Num();
    if (TouchedTriggerCount >= FMath::Max(1, RequiredTriggerCount))
    {
        RevealOneUp();
    }
}

void ASM64HiddenOneUp::RevealOneUp()
{
    if (!bActEnabled || bRevealed)
    {
        return;
    }
    bRevealed = true;
    if (!bRevealOffsetApplied)
    {
        SetActorLocation(HomeLocation + RevealOffset, false, nullptr, ETeleportType::TeleportPhysics);
        bRevealOffsetApplied = true;
    }
    SetActorHiddenInGame(false);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        HorizontalDirection = GetActorLocation() - Pawn->GetActorLocation();
        HorizontalDirection.Z = 0.0f;
        HorizontalDirection = HorizontalDirection.GetSafeNormal();
    }
    if (HorizontalDirection.IsNearlyZero())
    {
        HorizontalDirection = GetActorForwardVector().GetSafeNormal2D();
    }
}

void ASM64HiddenOneUp::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRevealed || !bActEnabled)
    {
        return;
    }
    const float StepSeconds = 1.0f / FMath::Max(1.0f, SimulationHz);
    AccumulatedSeconds += DeltaSeconds;
    while (AccumulatedSeconds >= StepSeconds)
    {
        AccumulatedSeconds -= StepSeconds;
        SimulateFrame();
    }
}

void ASM64HiddenOneUp::SimulateFrame()
{
    FVector Delta = FVector::ZeroVector;
    if (!bTangible)
    {
        float VerticalSpeed = 40.0f;
        float HorizontalSpeed = 0.0f;
        if (ActionFrame >= 5)
        {
            MoveAnglePitchUnits -= 0x1000;
            const float PitchRadians = static_cast<float>(MoveAnglePitchUnits) * (2.0f * PI / 65536.0f);
            VerticalSpeed = FMath::Cos(PitchRadians) * 30.0f + 2.0f;
            HorizontalSpeed = -FMath::Sin(PitchRadians) * 30.0f;
        }
        Delta = HorizontalDirection * HorizontalSpeed;
        Delta.Z = VerticalSpeed;
        if (ActionFrame >= 37)
        {
            bTangible = true;
            Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
    }
    else if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        if (bHomeTowardPlayer)
        {
            const FVector Target = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
            Delta = (Target - GetActorLocation()).GetSafeNormal() * 30.0f;
        }
        else
        {
            Delta = GetActorLocation() - Pawn->GetActorLocation();
            Delta.Z = 0.0f;
            Delta = Delta.GetSafeNormal() * 8.0f;
        }
    }
    FHitResult Hit;
    SetActorLocation(GetActorLocation() + Delta, true, &Hit, ETeleportType::None);
    ++ActionFrame;
}

ASM64HiddenOneUpTrigger::ASM64HiddenOneUpTrigger()
{
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASM64HiddenOneUpTrigger::OnTriggerOverlap);
}

void ASM64HiddenOneUpTrigger::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TriggerBox->SetBoxExtent(BoxExtent);
}

void ASM64HiddenOneUpTrigger::ResetForAct_Implementation()
{
    bTouched = false;
    TriggerBox->SetCollisionEnabled(bActEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ASM64HiddenOneUpTrigger::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (bTouched || !OtherActor || !OtherActor->IsA<APawn>() || !GetWorld())
    {
        return;
    }
    ASM64HiddenOneUp* Nearest = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<ASM64HiddenOneUp> It(GetWorld()); It; ++It)
    {
        if (It->TriggerGroup != TriggerGroup)
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            Nearest = *It;
            NearestDistanceSquared = DistanceSquared;
        }
    }
    if (Nearest)
    {
        bTouched = true;
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Nearest->RegisterHiddenTrigger(TriggerIndex);
    }
}
