#include "SpyroClimbingComponent.h"

#include "SpyroClimbSurface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Templates/UnrealTemplate.h"

namespace
{
    FVector2D FourWayInput(FVector2D Input)
    {
        if (!FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y)) { return FVector2D::ZeroVector; }
        return FMath::Abs(Input.X) > FMath::Abs(Input.Y)
            ? FVector2D(FMath::Clamp(Input.X, -1.f, 1.f), 0.f)
            : FVector2D(0.f, FMath::Clamp(Input.Y, -1.f, 1.f));
    }
}

USpyroClimbingComponent::USpyroClimbingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bAutoActivate = true;
}

void USpyroClimbingComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpyroClimbing requires an ACharacter owner: %s"), *GetNameSafe(GetOwner()));
        SetComponentTickEnabled(false);
        return;
    }
    Movement = Character->GetCharacterMovement();
    AddTickPrerequisiteActor(Character);
    if (Movement)
    {
        AddTickPrerequisiteComponent(Movement);
    }
    Character->OnTakeAnyDamage.AddDynamic(this, &USpyroClimbingComponent::HandleAnyDamage);
}

void USpyroClimbingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopClimbing(ESpyroClimbExitReason::Cancelled, false);
    if (IsValid(Character))
    {
        Character->OnTakeAnyDamage.RemoveDynamic(this, &USpyroClimbingComponent::HandleAnyDamage);
    }
    Super::EndPlay(EndPlayReason);
}

void USpyroClimbingComponent::Deactivate()
{
    Super::Deactivate();
    CancelClimbing();
}

ASpyroClimbSurface* USpyroClimbingComponent::GetCurrentSurface() const
{
    return CurrentSurface.Get();
}

bool USpyroClimbingComponent::CanStart() const
{
    return IsActive() && IsComponentTickEnabled() && bClimbingAllowed && !bClimbing && !bTransitioning
        && IsValid(Character) && IsValid(Movement) && GetWorld()
        && GetWorld()->GetNetMode() == NM_Standalone
        && GetWorld()->GetTimeSeconds() >= NextGrabTime
        && Movement->IsActive() && Movement->IsComponentTickEnabled()
        && (Movement->IsMovingOnGround() || Movement->IsFalling())
        && !Movement->bConstrainToPlane && !Character->bIsCrouched
        && !Character->GetAttachParentActor()
        && Character->GetRootComponent() == Character->GetCapsuleComponent()
        && Character->GetCapsuleComponent()->IsQueryCollisionEnabled()
        && !Character->GetCapsuleComponent()->IsSimulatingPhysics()
        && Character->GetActorUpVector().Equals(FVector::UpVector, KINDA_SMALL_NUMBER);
}

bool USpyroClimbingComponent::OwnsMovementMode() const
{
    return IsValid(Movement) && Movement->MovementMode == MOVE_Custom
        && Movement->CustomMovementMode == ClimbingCustomMode;
}

float USpyroClimbingComponent::GetHorizontalSupportRadius() const
{
    const float Margin = FMath::IsFinite(HorizontalEdgeMargin) ? FMath::Clamp(HorizontalEdgeMargin, 0.f, 1.f) : 1.f;
    return Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * Margin;
}

bool USpyroClimbingComponent::GetGrabTarget(ASpyroClimbSurface* Surface, FVector& OutTarget, FSpyroClimbFrame& OutFrame) const
{
    if (!IsValid(Surface) || !Surface->bClimbable || !Surface->HasSupportedTransform())
    {
        return false;
    }

    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Location = Character->GetActorLocation();
    const float Reach = HalfHeight + FMath::Max(0.f, GrabDistance) + FMath::Max(1.f, WallGap);
    if (!Surface->FindClimbFrame(Location, Character->GetActorForwardVector(), Reach, MinimumFacingDot, OutFrame))
    {
        return false;
    }
    const float Distance = FVector::DotProduct(Location - OutFrame.Origin, OutFrame.Normal);
    const FVector2D Coordinates = OutFrame.ToCoordinates(Location);
    const float StandOff = OutFrame.CapsuleStandOff(Radius, HalfHeight, WallGap);
    const float CapsuleExtent = OutFrame.CapsuleStandOff(Radius, HalfHeight, 1.f) - 1.f;
    if (Distance < CapsuleExtent || Distance > StandOff + FMath::Max(0.f, GrabDistance)
        || FVector::DotProduct(Character->GetActorForwardVector(), -OutFrame.Normal.GetSafeNormal2D()) < MinimumFacingDot)
    {
        return false;
    }

    OutTarget = OutFrame.ToWorld(Coordinates, StandOff);
    if (!Surface->FitClimbPosition(OutTarget, OutFrame, OutFrame.Normal, Radius, HalfHeight, WallGap, GetHorizontalSupportRadius()))
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbGrab), false, Character);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    const FCollisionShape Shape = Capsule->GetCollisionShape();
    const FQuat Rotation = OutFrame.FacingRotation();
    // Check both the whole approach and the destination; never teleport through an obstacle.
    FHitResult Hit;
    return !GetWorld()->SweepSingleByChannel(Hit, Location, OutTarget, Rotation, Capsule->GetCollisionObjectType(), Shape, Params, Responses)
        && !GetWorld()->OverlapBlockingTestByChannel(OutTarget, Rotation, Capsule->GetCollisionObjectType(), Shape, Params, Responses);
}

bool USpyroClimbingComponent::TryStartClimbingNearby()
{
    if (!CanStart())
    {
        return false;
    }
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbFind), true, Character);
    const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
        + FMath::Max(0.f, GrabDistance) + FMath::Max(1.f, WallGap) + 1.f;
    GetWorld()->OverlapMultiByObjectType(Overlaps, Character->GetActorLocation(), FQuat::Identity,
        Objects, FCollisionShape::MakeSphere(Radius), Params);

    ASpyroClimbSurface* Best = nullptr;
    float BestDistance = MAX_flt;
    TSet<ASpyroClimbSurface*> Examined;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        ASpyroClimbSurface* Surface = Cast<ASpyroClimbSurface>(Overlap.GetActor());
        if (!Surface || Examined.Contains(Surface)) { continue; }
        Examined.Add(Surface);
        FVector Target;
        FSpyroClimbFrame Frame;
        if (GetGrabTarget(Surface, Target, Frame))
        {
            const float Distance = FVector::DistSquared(Target, Character->GetActorLocation());
            if (Distance < BestDistance)
            {
                Best = Surface;
                BestDistance = Distance;
            }
        }
    }
    return Best && TryStartClimbing(Best);
}

bool USpyroClimbingComponent::TryStartClimbing(ASpyroClimbSurface* Surface)
{
    FVector Target;
    FSpyroClimbFrame Frame;
    if (!CanStart() || !GetGrabTarget(Surface, Target, Frame))
    {
        return false;
    }
    // The swept move can call Blueprint overlap/hit handlers. Block recursive entry.
    {
        TGuardValue<bool> Guard(bTransitioning, true);
        const uint32 EntrySerial = InterruptionSerial;
        FHitResult Hit;
        Character->GetCapsuleComponent()->MoveComponent(Target - Character->GetActorLocation(),
            Frame.FacingRotation(), true, &Hit);
        if (!IsValid(Character) || !IsValid(Movement) || !IsValid(Surface) || !bClimbingAllowed
            || !IsActive() || !IsComponentTickEnabled() || !Surface->bClimbable
            || !(Movement->IsMovingOnGround() || Movement->IsFalling())
            || !Movement->IsActive() || !Movement->IsComponentTickEnabled()
            || EntrySerial != InterruptionSerial
            || GetWorld()->GetTimeSeconds() < NextGrabTime || Hit.bBlockingHit
            || !Character->GetActorLocation().Equals(Target, .1f))
        {
            return false;
        }

        bSavedMovementTick = Movement->IsComponentTickEnabled();
        bSavedControllerYaw = Character->bUseControllerRotationYaw;
        bSavedControllerPitch = Character->bUseControllerRotationPitch;
        bSavedControllerRoll = Character->bUseControllerRotationRoll;
        bSavedOrientToMovement = Movement->bOrientRotationToMovement;
        bSavedUseDesiredRotation = Movement->bUseControllerDesiredRotation;
        ClimbingCharacterMesh = Character->GetMesh();
        bMeshWasOriented = false;
        if (USkeletalMeshComponent* Mesh = ClimbingCharacterMesh.Get())
        {
            SavedMeshTransform = Mesh->GetRelativeTransform();
            SavedMeshActorTransform = Mesh->GetComponentTransform().GetRelativeTransform(Character->GetActorTransform());
        }
        CurrentSurface = Surface;
        TrackSurface(Surface);
        ClimbFrame = Frame;
        bClimbing = true;
        PendingInput = ClimbInput = ClimbVelocity = FVector2D::ZeroVector;
        UpdateClimbAnimation(0.f);

        Character->StopJumping();
        Character->ConsumeMovementInputVector();
        Character->bUseControllerRotationYaw = false;
        Character->bUseControllerRotationPitch = false;
        Character->bUseControllerRotationRoll = false;
        Movement->bOrientRotationToMovement = false;
        Movement->bUseControllerDesiredRotation = false;
        Movement->StopMovementImmediately();
        Movement->ClearAccumulatedForces();
        Movement->SetComponentTickEnabled(false);
        // MOVE_None discards LaunchCharacter/AddImpulse calls in UE 4.27. A custom marker
        // with ticking suspended isolates movement while keeping damage knockback queueable.
        Movement->SetMovementMode(MOVE_Custom, ClimbingCustomMode);
    }
    // Mode-change Blueprint callbacks may have cancelled the climb already.
    if (bClimbing && UpdateMeshAlignment())
    {
        OnClimbStarted.Broadcast(Surface);
    }
    return bClimbing;
}

void USpyroClimbingComponent::AddClimbInput(FVector2D Input)
{
    if (bClimbing && !bTransferring && FMath::IsFinite(Input.X) && FMath::IsFinite(Input.Y))
    {
        PendingInput += Input;
    }
}

void USpyroClimbingComponent::UpdateClimbAnimation(float ProgressSpeed)
{
    // Geometry corrections must never pick a different animation direction. Only the input axis
    // actually selected by four-way movement determines direction; progress detects a blocked/idle pose.
    // Small speed hysteresis prevents sub-centimetre collision/edge noise from restarting a move loop.
    const float MinimumSpeed = ClimbDirection == ESpyroClimbDirection::Idle ? 1.f : .5f;
    if (!bClimbing || bTransferring || ClimbInput.IsNearlyZero()
        || !FMath::IsFinite(ProgressSpeed) || ProgressSpeed < MinimumSpeed)
    {
        ClimbDirection = ESpyroClimbDirection::Idle;
        ClimbAnimationSpeed = 0.f;
        return;
    }
    ClimbDirection = ClimbInput.X != 0.f
        ? (ClimbInput.X > 0.f ? ESpyroClimbDirection::Right : ESpyroClimbDirection::Left)
        : (ClimbInput.Y > 0.f ? ESpyroClimbDirection::Up : ESpyroClimbDirection::Down);
    ClimbAnimationSpeed = ProgressSpeed;
}

void USpyroClimbingComponent::TrackSurface(ASpyroClimbSurface* Surface)
{
    SurfaceTransform = Surface->GetGeometryTransform();
    bClimbingMeshCollision = Surface->UsesMeshCollision();
    ClimbingMesh = Surface->WallMesh->GetStaticMesh();
}

void USpyroClimbingComponent::RestoreMeshAlignment()
{
    if (!bMeshWasOriented) { return; }
    // Clear ownership first: changing the mesh transform can itself invoke gameplay callbacks.
    bMeshWasOriented = false;
    if (USkeletalMeshComponent* Mesh = ClimbingCharacterMesh.Get())
    {
        Mesh->SetRelativeTransform(SavedMeshTransform);
    }
}

bool USpyroClimbingComponent::UpdateMeshAlignment()
{
    if (!ValidateClimb()) { return false; }
    TGuardValue<bool> Guard(bTransitioning, true);
    if (!bOrientMeshToSurface)
    {
        RestoreMeshAlignment();
        return bClimbing && ValidateClimb();
    }
    USkeletalMeshComponent* Mesh = ClimbingCharacterMesh.Get();
    if (!Mesh) { return true; }

    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float CylinderHalfHeight = FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
    FQuat Rotation = FRotationMatrix::MakeFromXZ(-ClimbFrame.Normal, ClimbFrame.Up).ToQuat();
    FVector ClearanceCorrection = ClimbFrame.Normal * (CylinderHalfHeight * FMath::Abs(ClimbFrame.Normal.Z));
    if (bTransferring)
    {
        const FQuat LandingRotation = FRotationMatrix::MakeFromXZ(-TransferFrame.Normal, TransferFrame.Up).ToQuat();
        Rotation = FQuat::Slerp(Rotation, LandingRotation, TransferProgress).GetNormalized();
        const FVector LandingCorrection = TransferFrame.Normal * (CylinderHalfHeight * FMath::Abs(TransferFrame.Normal.Z));
        ClearanceCorrection = FMath::Lerp(ClearanceCorrection, LandingCorrection, TransferProgress);
    }
    // The upright capsule needs extra slope clearance. Remove only that extra distance from the
    // visual origin so the tilted pose keeps its usual distance from the wall. Facet clearance stays.
    const FVector VisualOrigin = Character->GetActorLocation() - ClearanceCorrection;
    const FVector Offset = Rotation.RotateVector(SavedMeshActorTransform.GetLocation() * Character->GetActorScale3D());
    bMeshWasOriented = true;
    Mesh->SetWorldLocationAndRotation(VisualOrigin + Offset, Rotation * SavedMeshActorTransform.GetRotation());
    // Damage/overlap handlers may have exited and restored the mesh during the transform update.
    return bClimbing && ValidateClimb();
}

bool USpyroClimbingComponent::ValidateClimb()
{
    if (!bClimbing) { return false; }
    if (!IsValid(Character) || !IsValid(Movement) || !bClimbingAllowed || !IsActive() || !IsComponentTickEnabled())
    {
        CancelClimbing();
        return false;
    }
    ASpyroClimbSurface* Surface = bTransferring ? TransferTarget.Get() : CurrentSurface.Get();
    // Moving panels are intentionally unsupported; release instead of dragging/teleporting the player.
    if (!Surface || !Surface->bClimbable || !Surface->HasSupportedTransform()
        || !Surface->GetGeometryTransform().Equals(SurfaceTransform, .01f)
        || Surface->UsesMeshCollision() != bClimbingMeshCollision
        || (bClimbingMeshCollision && Surface->WallMesh->GetStaticMesh() != ClimbingMesh.Get()))
    {
        StopClimbing(ESpyroClimbExitReason::SurfaceLost);
        return false;
    }
    if (!OwnsMovementMode() || Movement->IsComponentTickEnabled() || !Movement->IsActive()
        || Movement->bConstrainToPlane || Character->bIsCrouched
        || Character->GetRootComponent() != Character->GetCapsuleComponent()
        || !Character->GetActorUpVector().Equals(FVector::UpVector, KINDA_SMALL_NUMBER)
        || Character->GetAttachParentActor() || Character->GetCapsuleComponent()->IsSimulatingPhysics()
        || !Character->GetCapsuleComponent()->IsQueryCollisionEnabled())
    {
        StopClimbing(ESpyroClimbExitReason::Interrupted);
        return false;
    }
    const FVector Start = Character->GetActorLocation();
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (bTransferring)
    {
        if (!Start.Equals(TransferLastLocation, .5f))
        {
            StopClimbing(ESpyroClimbExitReason::Interrupted);
            return false;
        }
        const float LandingStandOff = TransferFrame.CapsuleStandOff(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), WallGap);
        if (!TransferTargetLocation.Equals(TransferFrame.ToWorld(TransferFrame.ToCoordinates(TransferTargetLocation), LandingStandOff), 2.f)
            || !Surface->SupportsPosition(TransferTargetLocation, TransferFrame,
            GetHorizontalSupportRadius(), Capsule->GetScaledCapsuleHalfHeight()))
        {
            StopClimbing(ESpyroClimbExitReason::SurfaceLost);
            return false;
        }
        return true;
    }
    const float StandOff = ClimbFrame.CapsuleStandOff(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), WallGap);
    const FVector2D Coordinates = ClimbFrame.ToCoordinates(Start);
    if (!Start.Equals(ClimbFrame.ToWorld(Coordinates, StandOff), 2.f)
        || !Surface->SupportsPosition(Start, ClimbFrame, GetHorizontalSupportRadius(), Capsule->GetScaledCapsuleHalfHeight()))
    {
        StopClimbing(ESpyroClimbExitReason::Interrupted);
        return false;
    }
    return true;
}

void USpyroClimbingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bClimbing)
    {
        PendingInput = FVector2D::ZeroVector;
        if (bAutoGrab) { TryStartClimbingNearby(); }
        return;
    }
    if (!ValidateClimb()) { return; }
    if (bTransferring)
    {
        TickTransfer(DeltaTime);
        return;
    }
    ASpyroClimbSurface* Surface = CurrentSurface.Get();
    const FVector Start = Character->GetActorLocation();
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float StandOff = ClimbFrame.CapsuleStandOff(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), WallGap);
    const FVector2D Coordinates = ClimbFrame.ToCoordinates(Start);
    ClimbInput = FourWayInput(PendingInput);
    PendingInput = FVector2D::ZeroVector;
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= SMALL_NUMBER)
    {
        UpdateClimbAnimation(0.f);
        UpdateMeshAlignment();
        return;
    }
    const FVector2D DesiredVelocity(ClimbInput.X * FMath::Max(0.f, HorizontalClimbingSpeed),
        ClimbInput.Y * FMath::Max(0.f, ClimbSpeed));
    if (bClimbingMeshCollision)
    {
        TickMeshClimb(DesiredVelocity, DeltaTime);
        return;
    }
    FVector2D Next = Coordinates + DesiredVelocity * DeltaTime;
    FVector2D Extents;
    Surface->GetUsableExtents(GetHorizontalSupportRadius(), Capsule->GetScaledCapsuleHalfHeight(), Extents);
    Next.X = FMath::Clamp(Next.X, -Extents.X, Extents.X);
    Next.Y = FMath::Clamp(Next.Y, -Extents.Y, Extents.Y);
    const FVector Target = ClimbFrame.ToWorld(Next, StandOff);
    const FVector Delta = Target - Start;
    Character->ConsumeMovementInputVector();
    FHitResult Hit;
    TGuardValue<bool> Guard(bTransitioning, true);
    Character->GetCapsuleComponent()->MoveComponent(Delta,
        ClimbFrame.FacingRotation(), true, &Hit);
    // Damage/trigger callbacks may exit during the sweep. Never overwrite their knockback afterward.
    if (!bClimbing || !IsValid(Character) || !IsValid(Movement))
    {
        return;
    }
    if (!IsValid(Surface) || !OwnsMovementMode())
    {
        StopClimbing(ESpyroClimbExitReason::Interrupted);
        return;
    }
    if (Hit.bStartPenetrating)
    {
        StopClimbing(ESpyroClimbExitReason::Interrupted);
        return;
    }
    Movement->Velocity = (Character->GetActorLocation() - Start) / DeltaTime;
    Movement->UpdateComponentVelocity();
    ClimbVelocity = FVector2D(FVector::DotProduct(Movement->Velocity, ClimbFrame.Right),
        FVector::DotProduct(Movement->Velocity, ClimbFrame.Up));
    const FVector InputDirection = (ClimbFrame.Right * ClimbInput.X + ClimbFrame.Up * ClimbInput.Y).GetSafeNormal();
    // A blocking hit means motion has stopped, even if sweep pullback leaves a tiny final displacement.
    UpdateClimbAnimation(Hit.bBlockingHit ? 0.f : FVector::DotProduct(Movement->Velocity, InputDirection));
    UpdateMeshAlignment();
}

void USpyroClimbingComponent::TickMeshClimb(FVector2D DesiredVelocity, float DeltaTime)
{
    ASpyroClimbSurface* Surface = CurrentSurface.Get();
    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Start = Character->GetActorLocation();
    const float StepSize = FMath::Min(5.f, FMath::Max(1.f, Radius * .25f));
    float Remaining = FMath::Min(DesiredVelocity.Size() * DeltaTime, 128.f * StepSize);
    float ProgressDistance = 0.f;
    bool bMovementBlocked = false;
    Character->ConsumeMovementInputVector();
    // Follow the local frame and sweep every short segment; a single long chord would cut into a bend.
    for (int32 Step = 0; Step < 128 && Remaining > SMALL_NUMBER; ++Step)
    {
        const float Distance = FMath::Min(Remaining, StepSize);
        Remaining -= Distance;
        const FVector Before = Character->GetActorLocation();
        const FVector Delta = (ClimbFrame.Right * DesiredVelocity.X + ClimbFrame.Up * DesiredVelocity.Y).GetSafeNormal() * Distance;
        FSpyroClimbFrame NextFrame = ClimbFrame;
        const FVector Target = Surface->ConstrainMeshMove(Before, Delta, NextFrame,
            Radius, HalfHeight, WallGap, GetHorizontalSupportRadius());
        if (Target.Equals(Before, .001f)) { break; }
        FHitResult Hit;
        {
            TGuardValue<bool> Guard(bTransitioning, true);
            Capsule->MoveComponent(Target - Before, NextFrame.FacingRotation(), true, &Hit);
        }
        if (!bClimbing || !IsValid(Character) || !IsValid(Movement)) { return; }
        if (!IsValid(Surface) || !OwnsMovementMode() || Hit.bStartPenetrating)
        {
            StopClimbing(ESpyroClimbExitReason::Interrupted);
            return;
        }
        const bool bReached = Character->GetActorLocation().Equals(Target, .1f);
        if (bReached) { ClimbFrame = NextFrame; }
        if (!ValidateClimb()) { return; }
        // Measure against this step's commanded tangent, before the frame changes. Projecting
        // the whole tick onto its final frame mixes in clearance adjustments on faceted walls.
        ProgressDistance += FMath::Max(0.f, FVector::DotProduct(Character->GetActorLocation() - Before, Delta.GetSafeNormal()));
        if (Hit.bBlockingHit || !bReached)
        {
            bMovementBlocked = true;
            break;
        }
        // Constraining an edge may produce a partial final step. Don't repeatedly push the boundary.
        if (FVector::DotProduct(Target - Before, Delta.GetSafeNormal()) < Distance - .05f) { break; }
    }
    Movement->Velocity = (Character->GetActorLocation() - Start) / DeltaTime;
    Movement->UpdateComponentVelocity();
    ClimbVelocity = FVector2D(FVector::DotProduct(Movement->Velocity, ClimbFrame.Right),
        FVector::DotProduct(Movement->Velocity, ClimbFrame.Up));
    UpdateClimbAnimation(bMovementBlocked ? 0.f : ProgressDistance / DeltaTime);
    UpdateMeshAlignment();
}

void USpyroClimbingComponent::StartRegrabDelay()
{
    if (GetWorld())
    {
        NextGrabTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, RegrabDelay);
    }
}

void USpyroClimbingComponent::StopClimbing(ESpyroClimbExitReason Reason, bool bBroadcast)
{
    if (!bClimbing)
    {
        return;
    }
    TGuardValue<bool> Guard(bTransitioning, true);
    bClimbing = false;
    bTransferring = false;
    bLargeTransfer = false;
    TransferTarget.Reset();
    TransferProgress = 0.f;
    CurrentSurface.Reset();
    ClimbingMesh.Reset();
    PendingInput = ClimbInput = ClimbVelocity = FVector2D::ZeroVector;
    UpdateClimbAnimation(0.f);
    StartRegrabDelay();
    RestoreMeshAlignment();
    ClimbingCharacterMesh.Reset();
    if (IsValid(Character) && IsValid(Movement))
    {
        Character->bUseControllerRotationYaw = bSavedControllerYaw;
        Character->bUseControllerRotationPitch = bSavedControllerPitch;
        Character->bUseControllerRotationRoll = bSavedControllerRoll;
        Movement->bOrientRotationToMovement = bSavedOrientToMovement;
        Movement->bUseControllerDesiredRotation = bSavedUseDesiredRotation;
        Character->ConsumeMovementInputVector();
        // Do not clear impulses/launches or velocity here: the damage system may have just set them.
        // An external mode change (swimming/death/etc.) owns the new mode.
        if (OwnsMovementMode())
        {
            Movement->SetMovementMode(MOVE_Falling);
        }
        Movement->SetComponentTickEnabled(bSavedMovementTick);
    }
    if (bBroadcast)
    {
        OnClimbEnded.Broadcast(Reason);
    }
}

bool USpyroClimbingComponent::JumpOff()
{
    if (!bClimbing || bTransitioning)
    {
        return false;
    }
    if (!ValidateClimb()) { return true; }
    const FVector Away = ClimbFrame.Normal;
    TGuardValue<bool> Guard(bTransitioning, true);
    // Set the launch first so exit/mode-change Blueprint callbacks retain the last word.
    const uint32 LaunchSerial = InterruptionSerial;
    Movement->ClearAccumulatedForces();
    Character->StopJumping();
    Character->LaunchCharacter(Away * FMath::Max(0.f, JumpAwaySpeed)
        + FVector::UpVector * FMath::Max(0.f, JumpUpSpeed), true, true);
    // OnLaunched can itself accept damage/cancel; do not emit a second, misleading exit event.
    if (bClimbing && LaunchSerial == InterruptionSerial)
    {
        StopClimbing(ESpyroClimbExitReason::Jumped);
    }
    return true;
}

bool USpyroClimbingComponent::JumpOrTransfer(FVector2D Input)
{
    if (!bClimbing || bTransitioning) { return false; }
    if (!ValidateClimb()) { return true; }
    if (bTransferring) { return true; }
    Input = FourWayInput(Input);
    if (Input.Size() >= FMath::Max(.01f, FMath::Clamp(JumpInputThreshold, 0.f, 1.f)))
    {
        if (bDropOffOnDownJump && Input.Y < 0.f)
        {
            TGuardValue<bool> Guard(bTransitioning, true);
            Character->StopJumping();
            // Discard climbing velocity so release starts from rest. Do not add a launch/impulse,
            // or clear external queued knockback. Normal Falling physics takes over on exit.
            Movement->StopMovementImmediately();
            StopClimbing(ESpyroClimbExitReason::DroppedOff);
            return true;
        }
        if (TryTransfer(Input)) { return true; }
    }
    return JumpOff();
}

void USpyroClimbingComponent::NotifyDamaged()
{
    ++InterruptionSerial;
    StartRegrabDelay();
    StopClimbing(ESpyroClimbExitReason::Damaged);
}

void USpyroClimbingComponent::CancelClimbing()
{
    ++InterruptionSerial;
    StopClimbing(ESpyroClimbExitReason::Cancelled);
}

void USpyroClimbingComponent::SetClimbingAllowed(bool bAllowed)
{
    bClimbingAllowed = bAllowed;
    if (!bAllowed)
    {
        CancelClimbing();
    }
}

void USpyroClimbingComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
    AController* InstigatedBy, AActor* DamageCauser)
{
    if (bCancelOnAnyDamage && Damage > 0.f)
    {
        NotifyDamaged();
    }
}
