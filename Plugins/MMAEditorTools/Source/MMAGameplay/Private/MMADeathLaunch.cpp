#include "MMADeathLaunch.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/EngineTypes.h"

namespace
{
void UnstickFromFloor(ACharacter* Character, float UnstickHeight)
{
    if (!Character || UnstickHeight <= 0.0f)
    {
        return;
    }
    Character->SetActorLocation(
        Character->GetActorLocation() + FVector(0.0f, 0.0f, UnstickHeight),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

void ForceFallingLaunch(ACharacter* Character, const FVector& Velocity)
{
    if (!Character)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Falling);
        Character->LaunchCharacter(Velocity, true, true);
        Movement->Velocity = Velocity;
        Movement->UpdateComponentVelocity();
    }
    else
    {
        Character->LaunchCharacter(Velocity, true, true);
    }
}
}

FVector FMMADeathLaunch::AwayFromActor(const AActor* Self, const AActor* Source)
{
    FVector Away = FVector::ForwardVector;
    if (Self && Source)
    {
        Away = Self->GetActorLocation() - Source->GetActorLocation();
    }
    else if (Self)
    {
        Away = -Self->GetActorForwardVector();
    }
    Away.Z = 0.0f;
    if (!Away.Normalize() && Self)
    {
        Away = -Self->GetActorForwardVector();
        Away.Z = 0.0f;
        Away.Normalize();
    }
    return Away;
}

void FMMADeathLaunch::Begin(
    ACharacter* Character,
    const FVector& Velocity,
    float GravityScale,
    float InHoldSeconds,
    float UnstickHeight)
{
    if (!Character)
    {
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (Movement && !bHasSavedMovement)
    {
        SavedGravityScale = Movement->GravityScale;
        SavedFallingLateralFriction = Movement->FallingLateralFriction;
        SavedBrakingDecelerationFalling = Movement->BrakingDecelerationFalling;
        SavedAirControl = Movement->AirControl;
        bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
        bHasSavedMovement = true;
    }
    if (Movement)
    {
        Movement->StopMovementImmediately();
        Movement->GravityScale = FMath::Max(GravityScale, 0.05f);
        Movement->FallingLateralFriction = 0.0f;
        Movement->BrakingDecelerationFalling = 0.0f;
        Movement->AirControl = 0.0f;
        Movement->bOrientRotationToMovement = false;
    }
    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }
    UnstickFromFloor(Character, UnstickHeight);
    ForceFallingLaunch(Character, Velocity);

    LaunchVelocity = Velocity;
    HoldSeconds = FMath::Max(InHoldSeconds, 0.0f);
    Elapsed = 0.0f;
    bActive = true;
}

void FMMADeathLaunch::Tick(ACharacter* Character, float DeltaTime)
{
    if (!bActive || !Character)
    {
        return;
    }
    Elapsed += DeltaTime;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement)
    {
        return;
    }
    if (Movement->MovementMode != MOVE_Falling)
    {
        Movement->SetMovementMode(MOVE_Falling);
    }
    FVector Velocity = Movement->Velocity;
    const bool bHolding = Elapsed <= HoldSeconds;
    if (bHolding)
    {
        Velocity.X = LaunchVelocity.X;
        Velocity.Y = LaunchVelocity.Y;
    }
    // First frames: floor snap often zeros Z even after SetMovementMode(Falling).
    if (Elapsed < 0.12f && Velocity.Z < LaunchVelocity.Z * 0.5f)
    {
        Velocity.Z = LaunchVelocity.Z;
    }
    Movement->Velocity = Velocity;
    Movement->UpdateComponentVelocity();
}

void FMMADeathLaunch::End(ACharacter* Character)
{
    bActive = false;
    if (!Character || !bHasSavedMovement)
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->GravityScale = SavedGravityScale;
        Movement->FallingLateralFriction = SavedFallingLateralFriction;
        Movement->BrakingDecelerationFalling = SavedBrakingDecelerationFalling;
        Movement->AirControl = SavedAirControl;
        Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
    }
}

void MMAImpulseLaunch(ACharacter* Character, const FVector& Velocity, float UnstickHeight)
{
    if (!Character)
    {
        return;
    }
    UnstickFromFloor(Character, UnstickHeight);
    ForceFallingLaunch(Character, Velocity);
}
