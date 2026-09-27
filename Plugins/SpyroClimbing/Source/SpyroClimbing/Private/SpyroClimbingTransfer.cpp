#include "SpyroClimbingComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Templates/UnrealTemplate.h"

FVector USpyroClimbingComponent::TransferPosition(float Alpha) const
{
    return FMath::Lerp(TransferStart, TransferTargetLocation, Alpha)
        + (ClimbFrame.Normal * ActiveArcHeight + FVector::UpVector * ActiveHorizontalArcHeight)
        * (4.f * Alpha * (1.f - Alpha));
}

bool USpyroClimbingComponent::PathClear(const FVector& From, const FVector& To) const
{
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbTransfer), false, Character);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    FHitResult Hit;
    return !GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity,
        Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Params, Responses)
        && !GetWorld()->OverlapBlockingTestByChannel(To, FQuat::Identity,
            Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Params, Responses);
}

bool USpyroClimbingComponent::TransferPathClear() const
{
    FVector Previous = TransferStart;
    for (int32 Step = 1; Step <= TransferSteps; ++Step)
    {
        const FVector Next = TransferPosition(static_cast<float>(Step) / TransferSteps);
        if (!PathClear(Previous, Next)) { return false; }
        Previous = Next;
    }
    return true;
}

bool USpyroClimbingComponent::TryTransfer(FVector2D Input)
{
    if (TryTransferWithProfile(Input, false)) { return true; }
    return bEnableLargeTransfers && Input.X != 0.f && TryTransferWithProfile(Input, true);
}

float USpyroClimbingComponent::ConfigureTransferArc(float Travel, float FullTravel, bool bHorizontal, bool bLarge)
{
    const float Fraction = FMath::Clamp(Travel / FullTravel, 0.f, 1.f);
    const float Scale = FMath::Sqrt(Fraction);
    ActiveArcHeight = FMath::Max(0.f, bLarge ? LargeTransferArcHeight : TransferArcHeight) * Scale;
    ActiveHorizontalArcHeight = bHorizontal
        ? FMath::Max(0.f, bLarge ? LargeTransferUpwardArcHeight : HorizontalTransferArcHeight) * Scale : 0.f;
    TransferSteps = FMath::Clamp(FMath::CeilToInt((Travel + 2.f * (ActiveArcHeight + ActiveHorizontalArcHeight)) / 8.f), 32, 512);
    return Fraction;
}

bool USpyroClimbingComponent::TryTransferWithProfile(FVector2D Input, bool bLarge)
{
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float NormalMaxDistance = FMath::Max(1.f, Input.X != 0.f ? HorizontalJumpDistance : VerticalJumpDistance);
    const float MaxDistance = bLarge ? FMath::Max(1.f, LargeTransferDistance) : NormalMaxDistance;
    if (bLarge && (MaxDistance <= NormalMaxDistance || LargeTransferHeightDrop <= 0.f)) { return false; }
    // Down along the source plane preserves contact on tilted walls; divide by Up.Z to express the
    // setting as world-height loss. Projection onto a different wall is checked again below.
    const float FullDropAlongWall = bLarge ? LargeTransferHeightDrop / FMath::Max(.01f, ClimbFrame.Up.Z) : 0.f;
    const float FullTravel = FMath::Sqrt(FMath::Square(MaxDistance) + FMath::Square(FullDropAlongWall));
    const float DesiredDistance = MaxDistance * Input.Size();
    const FVector Direction = (ClimbFrame.Right * Input.X + ClimbFrame.Up * Input.Y).GetSafeNormal();
    const float SearchTolerance = bLarge ? LargeTransferLandingSearchTolerance : LandingSearchTolerance;
    const float DepthTolerance = FMath::Max(0.f, bLarge ? LargeTransferDepthTolerance : TransferDepthTolerance);
    const float SearchSpan = FMath::Min(FMath::Max(0.f, SearchTolerance), DesiredDistance * .25f);
    const int32 Samples = FMath::Max(1, FMath::CeilToInt(SearchSpan / 5.f));
    TransferStart = Character->GetActorLocation();

    // Search the requested endpoint first, then slightly shorter landings. Never increase analog reach
    // or rescue a gap jump by attaching back to a distant edge near its departure point.
    for (int32 Sample = 0; Sample <= Samples; ++Sample)
    {
        const float Distance = DesiredDistance - SearchSpan * Sample / Samples;
        if (bLarge && Distance <= NormalMaxDistance * Input.Size() + 1.f) { break; }
        const FVector Desired = TransferStart + Direction * Distance
            - ClimbFrame.Up * (FullDropAlongWall * Distance / MaxDistance);
        ASpyroClimbSurface* Best = nullptr;
        FVector BestPosition = FVector::ZeroVector;
        FSpyroClimbFrame BestFrame;
        float BestDepth = BIG_NUMBER;
        for (TActorIterator<ASpyroClimbSurface> It(GetWorld()); It; ++It)
        {
            ASpyroClimbSurface* Candidate = *It;
            FVector Position;
            FSpyroClimbFrame Frame;
            // Deliberately include CurrentSurface: disconnected panels can share a mesh and pivot.
            if (!IsValid(Candidate) || !Candidate->FindTransferLanding(Desired, ClimbFrame.Normal,
                DepthTolerance, Radius, HalfHeight, WallGap, GetHorizontalSupportRadius(), Position, Frame)) { continue; }
            if (bLarge && (Position.Z >= TransferStart.Z - 1.f
                || FVector::DotProduct(Position - TransferStart, Direction) <= NormalMaxDistance * Input.Size() + 1.f)) { continue; }
            const float Depth = FMath::Abs(FVector::DotProduct(Position - Desired, ClimbFrame.Normal));
            const bool bBetter = Depth < BestDepth - .01f || (FMath::IsNearlyEqual(Depth, BestDepth, .01f)
                && Best && Candidate->GetPathName() < Best->GetPathName());
            if (!bBetter) { continue; }
            TransferTargetLocation = Position;
            const float Travel = FVector::Distance(TransferStart, Position);
            ConfigureTransferArc(Travel, FullTravel, Input.X != 0.f, bLarge);
            if (!TransferPathClear()) { continue; }
            Best = Candidate;
            BestPosition = Position;
            BestFrame = Frame;
            BestDepth = Depth;
        }
        if (!Best) { continue; }
        TransferTargetLocation = BestPosition;
        TransferFrame = BestFrame;
        const float Travel = FVector::Distance(TransferStart, BestPosition);
        const float Fraction = ConfigureTransferArc(Travel, FullTravel, Input.X != 0.f, bLarge);
        const float NormalMinSeconds = FMath::Max(.05f, MinTransferDuration);
        const float NormalMaxSeconds = FMath::Max(NormalMinSeconds, MaxTransferDuration);
        const float MinSeconds = bLarge ? FMath::Max(LargeTransferMinDuration, NormalMaxSeconds + .05f) : NormalMinSeconds;
        const float MaxSeconds = bLarge ? FMath::Max(MinSeconds, LargeTransferMaxDuration) : NormalMaxSeconds;
        TransferDuration = FMath::Lerp(MinSeconds, MaxSeconds, Fraction);
        TransferTarget = Best;
        TrackSurface(Best);
        TransferLastLocation = TransferStart;
        TransferElapsed = TransferProgress = 0.f;
        ClimbVelocity = PendingInput = FVector2D::ZeroVector;
        ClimbInput = Input;
        bTransferring = true;
        bLargeTransfer = bLarge;
        UpdateClimbAnimation(0.f);
        Character->ConsumeMovementInputVector();
        Movement->StopMovementImmediately();
        OnTransferStarted.Broadcast(Best);
        return true; // Consumed even if a Blueprint event immediately cancels/damages the character.
    }
    return false;
}

void USpyroClimbingComponent::TickTransfer(float DeltaTime)
{
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= SMALL_NUMBER) { return; }
    Character->ConsumeMovementInputVector();
    const float EndTime = FMath::Min(TransferElapsed + DeltaTime, TransferDuration);
    // Sweep short chords of the arc, including during hitches, instead of cutting across the wall.
    for (int32 Step = 0; Step <= TransferSteps && TransferElapsed < EndTime; ++Step)
    {
        const float NextTime = FMath::Min(TransferElapsed + TransferDuration / TransferSteps, EndTime);
        const float Alpha = NextTime / TransferDuration;
        const FVector Position = TransferPosition(Alpha);
        const FVector Start = Character->GetActorLocation();
        const FQuat Rotation = FQuat::Slerp(ClimbFrame.FacingRotation(), TransferFrame.FacingRotation(), Alpha).GetNormalized();
        // Set velocity before the sweep: an overlap's damage/knockback must never be overwritten.
        Movement->Velocity = (Position - Start) / FMath::Max(SMALL_NUMBER, NextTime - TransferElapsed);
        Movement->UpdateComponentVelocity();
        FHitResult Hit;
        {
            TGuardValue<bool> Guard(bTransitioning, true);
            Character->GetCapsuleComponent()->MoveComponent(Position - Start, Rotation, true, &Hit);
        }
        if (!bClimbing || !bTransferring || !IsValid(Character) || !IsValid(Movement)) { return; }
        if (Hit.bBlockingHit || !Character->GetActorLocation().Equals(Position, .1f))
        {
            StopClimbing(ESpyroClimbExitReason::Obstacle);
            return;
        }
        TransferLastLocation = Character->GetActorLocation();
        if (!ValidateClimb()) { return; }
        TransferElapsed = NextTime;
        TransferProgress = Alpha;
    }
    if (!UpdateMeshAlignment()) { return; }
    if (TransferElapsed >= TransferDuration - SMALL_NUMBER)
    {
        ASpyroClimbSurface* LandedSurface = TransferTarget.Get();
        CurrentSurface = LandedSurface;
        ClimbFrame = TransferFrame;
        TransferTarget.Reset();
        bTransferring = false;
        bLargeTransfer = false;
        TransferProgress = 1.f;
        ClimbInput = ClimbVelocity = PendingInput = FVector2D::ZeroVector;
        UpdateClimbAnimation(0.f);
        Movement->StopMovementImmediately();
        OnTransferLanded.Broadcast(LandedSurface);
        // No writes after broadcasting: handlers may exit or begin another jump.
    }
}
