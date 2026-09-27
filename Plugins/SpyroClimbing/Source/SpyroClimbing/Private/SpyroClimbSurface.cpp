#include "SpyroClimbSurface.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float MaxNormalZ = .70710678f; // 45 degrees from vertical, excluding floors/ceilings.
    constexpr float MinimumNeighbourNormalDot = .9f; // Gentle facets, not sharp corner wrapping.
    constexpr float MeshProjectionDepth = 20.f;

    bool IsClimbableNormal(const FVector& Normal)
    {
        return !Normal.IsNearlyZero() && FMath::Abs(Normal.Z) <= MaxNormalZ;
    }
}

ASpyroClimbSurface::ASpyroClimbSurface()
{
    PrimaryActorTick.bCanEverTick = false;
    ClimbArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbArea"));
    SetRootComponent(ClimbArea);
    ClimbArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ClimbArea->SetCollisionObjectType(ECC_WorldDynamic);
    ClimbArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    ClimbArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ClimbArea->SetGenerateOverlapEvents(false);
    ClimbArea->SetCanEverAffectNavigation(false);

    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    WallMesh->SetupAttachment(ClimbArea);
    WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        WallMesh->SetStaticMesh(Cube.Object);
    }

    OutwardArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutwardArrow"));
    OutwardArrow->SetupAttachment(ClimbArea);
    OutwardArrow->ArrowColor = FColor::Green;
    OutwardArrow->SetHiddenInGame(true);
    RefreshSurface();
}

void ASpyroClimbSurface::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshSurface();
}

void ASpyroClimbSurface::RefreshSurface()
{
    Width = FMath::Max(1.f, Width);
    Height = FMath::Max(1.f, Height);
    WallThickness = FMath::Max(1.f, WallThickness);
    ClimbArea->SetBoxExtent(FVector(1.f, Width * .5f, Height * .5f));
    if (bFitDefaultCube)
    {
        WallMesh->SetRelativeLocation(FVector(-WallThickness * .5f, 0.f, 0.f));
        WallMesh->SetRelativeRotation(FRotator::ZeroRotator);
        WallMesh->SetRelativeScale3D(FVector(WallThickness, Width, Height) / 100.f);
    }
}

bool ASpyroClimbSurface::HasSupportedTransform() const
{
    if (UsesMeshCollision())
    {
        const FVector Scale = WallMesh->GetComponentScale();
        return Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER && Scale.Z > SMALL_NUMBER
            && !WallMesh->IsSimulatingPhysics();
    }
    return GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER)
        && GetActorUpVector().Equals(FVector::UpVector, KINDA_SMALL_NUMBER);
}

void ASpyroClimbSurface::ResetWallMeshTransform()
{
    bFitDefaultCube = false;
    WallMesh->SetRelativeTransform(FTransform::Identity);
    RefreshSurface();
}

bool ASpyroClimbSurface::UsesMeshCollision() const
{
    return GeometryMode == ESpyroClimbGeometry::MeshCollision
        || (GeometryMode == ESpyroClimbGeometry::Automatic && !bFitDefaultCube);
}

FTransform ASpyroClimbSurface::GetGeometryTransform() const
{
    return UsesMeshCollision() ? WallMesh->GetComponentTransform() : GetActorTransform();
}

bool ASpyroClimbSurface::FindClimbFrame(const FVector& Location, const FVector& Facing, float Reach,
    float MinimumFacingDot, FSpyroClimbFrame& OutFrame) const
{
    if (!UsesMeshCollision())
    {
        OutFrame.Origin = GetActorLocation();
        OutFrame.Normal = GetOutwardNormal();
        OutFrame.Right = GetClimbRight();
        OutFrame.Up = GetClimbUp();
        return true;
    }
    if (!WallMesh->GetStaticMesh() || !WallMesh->IsQueryCollisionEnabled())
    {
        return false;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbMeshGrab), true);
    float BestDistance = MAX_flt;
    // Sample the approach cone. Each ray hits this component's actual triangles, never its bounds.
    // Both WorldStatic and WorldDynamic meshes are supported by the nearby spatial query.
    for (float Angle : { 0.f, -30.f, 30.f, -60.f, 60.f })
    {
        const FVector Direction = Facing.GetSafeNormal2D().RotateAngleAxis(Angle, FVector::UpVector);
        FHitResult Hit;
        if (!WallMesh->LineTraceComponent(Hit, Location, Location + Direction * Reach * 2.f, Params)
            || !IsClimbableNormal(Hit.ImpactNormal))
        {
            continue;
        }
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
        const float Distance = FVector::DotProduct(Location - Hit.ImpactPoint, Normal);
        if (Distance < 0.f || Distance > Reach || Hit.Distance >= BestDistance
            || FVector::DotProduct(Facing.GetSafeNormal2D(), -Normal.GetSafeNormal2D()) < MinimumFacingDot)
        {
            continue;
        }
        // Compare actual ray hit distances. A neighbouring facet's infinite plane may pass
        // closer to the capsule even though its real triangles are far off to the side.
        BestDistance = Hit.Distance;
        OutFrame.SetPlane(Hit.ImpactPoint, Normal);
    }
    return BestDistance < MAX_flt;
}

bool ASpyroClimbSurface::SupportsPosition(const FVector& Position, const FSpyroClimbFrame& Frame,
    float HorizontalSupportRadius, float CapsuleHalfHeight) const
{
    if (!UsesMeshCollision())
    {
        FVector2D Extents;
        const FVector2D Coordinates = Frame.ToCoordinates(Position);
        return GetUsableExtents(HorizontalSupportRadius, CapsuleHalfHeight, Extents)
            && FMath::Abs(Coordinates.X) <= Extents.X + .01f
            && FMath::Abs(Coordinates.Y) <= Extents.Y + .01f;
    }
    if (!WallMesh->GetStaticMesh() || !WallMesh->IsQueryCollisionEnabled())
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbMeshSupport), true);
    const FVector OnPlane = Frame.ToWorld(Frame.ToCoordinates(Position), 0.f);
    // Check the authored support width and full capsule height. Even with zero side margin,
    // the centre column needs real support; a merged mesh's empty gaps remain unclimbable.
    for (int32 Horizontal = -1; Horizontal <= 1; ++Horizontal)
    {
        for (int32 Vertical = -1; Vertical <= 1; ++Vertical)
        {
            const FVector Point = OnPlane + Frame.Right * (Horizontal * HorizontalSupportRadius)
                + Frame.Up * (Vertical * CapsuleHalfHeight);
            FHitResult Hit;
            if (!WallMesh->LineTraceComponent(Hit, Point + Frame.Normal * MeshProjectionDepth,
                Point - Frame.Normal * MeshProjectionDepth, Params)
                || !IsClimbableNormal(Hit.ImpactNormal)
                || FVector::DotProduct(Hit.ImpactNormal, Frame.Normal) < MinimumNeighbourNormalDot)
            {
                return false;
            }
        }
    }
    return true;
}

bool ASpyroClimbSurface::FitClimbPosition(FVector& InOutPosition, FSpyroClimbFrame& InOutFrame,
    const FVector& CorrectionDirection, float CapsuleRadius, float CapsuleHalfHeight, float WallGap,
    float HorizontalSupportRadius) const
{
    if (!SupportsPosition(InOutPosition, InOutFrame, HorizontalSupportRadius, CapsuleHalfHeight)) { return false; }
    if (!UsesMeshCollision()) { return true; }
    const FVector OnPlane = InOutFrame.ToWorld(InOutFrame.ToCoordinates(InOutPosition), 0.f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbFacetClearance), true);
    float Correction = 0.f;
    // Full body width for clearance, even if support allows side overhang. Missing samples here
    // are fine; support was checked separately. Final swept collision still tests the real capsule.
    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            const FVector Point = OnPlane + InOutFrame.Right * (X * CapsuleRadius) + InOutFrame.Up * (Y * CapsuleHalfHeight);
            FHitResult Hit;
            if (!WallMesh->LineTraceComponent(Hit, Point + InOutFrame.Normal * MeshProjectionDepth,
                Point - InOutFrame.Normal * MeshProjectionDepth, Params)
                || FVector::DotProduct(Hit.ImpactNormal, InOutFrame.Normal) < MinimumNeighbourNormalDot) { continue; }
            const float Alignment = FVector::DotProduct(CorrectionDirection, Hit.ImpactNormal);
            if (Alignment <= .5f) { continue; }
            const float Required = CapsuleRadius + FMath::Max(0.f, CapsuleHalfHeight - CapsuleRadius)
                * FMath::Abs(Hit.ImpactNormal.Z) + FMath::Max(1.f, WallGap);
            Correction = FMath::Max(Correction,
                (Required - FVector::DotProduct(InOutPosition - Hit.ImpactPoint, Hit.ImpactNormal)) / Alignment);
        }
    }
    if (Correction > MeshProjectionDepth) { return false; }
    InOutPosition += CorrectionDirection * Correction;
    InOutFrame.ClearanceOffset += Correction * FVector::DotProduct(CorrectionDirection, InOutFrame.Normal);
    return SupportsPosition(InOutPosition, InOutFrame, HorizontalSupportRadius, CapsuleHalfHeight);
}

bool ASpyroClimbSurface::FindTransferLanding(const FVector& DesiredPosition, const FVector& SourceNormal,
    float DepthTolerance, float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius,
    FVector& OutPosition, FSpyroClimbFrame& OutFrame) const
{
    if (!bClimbable || !HasSupportedTransform()) { return false; }
    // Adjacent panels may have different yaw, but this is not a corner/wraparound jump.
    constexpr float MinimumNormalDot = .7f;
    if (UsesMeshCollision())
    {
        if (!WallMesh->GetStaticMesh() || !WallMesh->IsQueryCollisionEnabled()) { return false; }
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbLanding), true);
        if (!WallMesh->LineTraceComponent(Hit, DesiredPosition + SourceNormal * DepthTolerance,
            DesiredPosition - SourceNormal * (DepthTolerance + (CapsuleHalfHeight + FMath::Max(1.f, WallGap)) / MinimumNormalDot), Params)
            || !IsClimbableNormal(Hit.ImpactNormal)) { return false; }
        OutFrame.SetPlane(Hit.ImpactPoint, Hit.ImpactNormal);
    }
    else
    {
        OutFrame.Origin = GetActorLocation();
        OutFrame.Normal = GetOutwardNormal();
        OutFrame.Right = GetClimbRight();
        OutFrame.Up = GetClimbUp();
    }
    const float Alignment = FVector::DotProduct(SourceNormal, OutFrame.Normal);
    if (Alignment < MinimumNormalDot) { return false; }
    const float StandOff = OutFrame.CapsuleStandOff(CapsuleRadius, CapsuleHalfHeight, WallGap);
    // Correct depth along the source normal, preserving the requested four-way distance exactly.
    const float Correction = (StandOff - FVector::DotProduct(DesiredPosition - OutFrame.Origin, OutFrame.Normal)) / Alignment;
    if (FMath::Abs(Correction) > DepthTolerance + .01f) { return false; }
    OutPosition = DesiredPosition + SourceNormal * Correction;
    return FitClimbPosition(OutPosition, OutFrame, SourceNormal, CapsuleRadius, CapsuleHalfHeight, WallGap, HorizontalSupportRadius)
        && FMath::Abs(FVector::DotProduct(OutPosition - DesiredPosition, SourceNormal)) <= DepthTolerance + .01f;
}

bool ASpyroClimbSurface::ProjectMeshPosition(const FVector& Desired, const FSpyroClimbFrame& PreviousFrame,
    float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius,
    FVector& OutPosition, FSpyroClimbFrame& OutFrame) const
{
    const FVector Probe = PreviousFrame.ToWorld(PreviousFrame.ToCoordinates(Desired), 0.f);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpyroClimbMeshProject), true);
    if (!WallMesh->LineTraceComponent(Hit, Probe + PreviousFrame.Normal * MeshProjectionDepth,
        Probe - PreviousFrame.Normal * MeshProjectionDepth, Params)
        || !IsClimbableNormal(Hit.ImpactNormal)) { return false; }
    OutFrame.SetPlane(Hit.ImpactPoint, Hit.ImpactNormal);
    const float Alignment = FVector::DotProduct(PreviousFrame.Normal, OutFrame.Normal);
    if (Alignment < MinimumNeighbourNormalDot) { return false; }
    const float StandOff = OutFrame.CapsuleStandOff(CapsuleRadius, CapsuleHalfHeight, WallGap);
    const float Correction = (StandOff - FVector::DotProduct(Desired - OutFrame.Origin, OutFrame.Normal)) / Alignment;
    OutPosition = Desired + PreviousFrame.Normal * Correction;
    return FitClimbPosition(OutPosition, OutFrame, PreviousFrame.Normal, CapsuleRadius, CapsuleHalfHeight, WallGap, HorizontalSupportRadius);
}

FVector ASpyroClimbSurface::ConstrainMeshMove(const FVector& Start, const FVector& DesiredDelta,
    FSpyroClimbFrame& InOutFrame, float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius) const
{
    if (DesiredDelta.IsNearlyZero()) { return Start; }
    const FSpyroClimbFrame PreviousFrame = InOutFrame;
    FSpyroClimbFrame NextFrame;
    FVector Target;
    if (ProjectMeshPosition(Start + DesiredDelta, PreviousFrame, CapsuleRadius, CapsuleHalfHeight,
        WallGap, HorizontalSupportRadius, Target, NextFrame))
    {
        InOutFrame = NextFrame;
        return Target;
    }
    float Low = 0.f, High = 1.f;
    FVector Result = Start;
    for (int32 Iteration = 0; Iteration < 8; ++Iteration)
    {
        const float Mid = (Low + High) * .5f;
        if (ProjectMeshPosition(Start + DesiredDelta * Mid, PreviousFrame, CapsuleRadius, CapsuleHalfHeight,
            WallGap, HorizontalSupportRadius, Target, NextFrame))
        {
            Low = Mid;
            Result = Target;
            InOutFrame = NextFrame;
        }
        else { High = Mid; }
    }
    return Result;
}

FVector ASpyroClimbSurface::GetOutwardNormal() const { return GetActorForwardVector(); }
FVector ASpyroClimbSurface::GetClimbRight() const { return -GetActorRightVector(); }
FVector ASpyroClimbSurface::GetClimbUp() const { return GetActorUpVector(); }

FVector2D ASpyroClimbSurface::WorldToClimbCoordinates(const FVector& Location) const
{
    const FVector Offset = Location - GetActorLocation();
    return FVector2D(FVector::DotProduct(Offset, GetClimbRight()), FVector::DotProduct(Offset, GetClimbUp()));
}

FVector ASpyroClimbSurface::ClimbCoordinatesToWorld(const FVector2D& Coordinates, float StandOff) const
{
    return GetActorLocation() + GetOutwardNormal() * StandOff
        + GetClimbRight() * Coordinates.X + GetClimbUp() * Coordinates.Y;
}

bool ASpyroClimbSurface::GetUsableExtents(float HorizontalSupportRadius, float CapsuleHalfHeight, FVector2D& OutExtents) const
{
    OutExtents = FVector2D(Width * .5f - HorizontalSupportRadius, Height * .5f - CapsuleHalfHeight);
    return OutExtents.X >= 0.f && OutExtents.Y >= 0.f;
}
