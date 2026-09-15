#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacter;

/**
 * Death fly-back that survives CharacterMovement floor snap.
 *
 * ACharacter::LaunchCharacter is a one-frame velocity write. MMA death is
 * PlayAnimation (no root motion into the capsule) plus native Dead AI that
 * can StopMovement / return to Walking on later ticks. Holding XY, forcing
 * Falling, unsticking from the floor, and dropping gravity for the clip
 * length is what actually throws the actor across the room.
 */
struct FMMADeathLaunch
{
    bool bActive = false;
    bool bHasSavedMovement = false;
    float SavedGravityScale = 1.0f;
    float SavedFallingLateralFriction = 0.0f;
    float SavedBrakingDecelerationFalling = 0.0f;
    float SavedAirControl = 0.05f;
    bool bSavedOrientRotationToMovement = true;
    FVector LaunchVelocity = FVector::ZeroVector;
    float HoldSeconds = 0.0f;
    float Elapsed = 0.0f;

    static FVector AwayFromActor(const AActor* Self, const AActor* Source);

    void Begin(
        ACharacter* Character,
        const FVector& Velocity,
        float GravityScale,
        float HoldSeconds,
        float UnstickHeight);

    void Tick(ACharacter* Character, float DeltaTime);

    void End(ACharacter* Character);
};

/** One-shot shove (charge hit, etc.): unstick + LaunchCharacter, no hold. */
void MMAImpulseLaunch(ACharacter* Character, const FVector& Velocity, float UnstickHeight);
