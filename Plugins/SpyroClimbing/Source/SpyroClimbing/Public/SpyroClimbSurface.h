#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpyroClimbSurface.generated.h"

class UArrowComponent;
class UBoxComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESpyroClimbGeometry : uint8
{
    Automatic,
    Rectangle,
    MeshCollision
};

/** A world-space plane acquired from a mesh hit or the authored rectangle. */
struct SPYROCLIMBING_API FSpyroClimbFrame
{
    FVector Origin = FVector::ZeroVector;
    FVector Normal = FVector::ForwardVector;
    FVector Right = -FVector::RightVector;
    FVector Up = FVector::UpVector;
    // Additional clearance required by neighbouring facets beneath the upright capsule.
    float ClearanceOffset = 0.f;

    void SetPlane(const FVector& Point, const FVector& PlaneNormal)
    {
        Origin = Point;
        Normal = PlaneNormal.GetSafeNormal();
        Right = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal();
        Up = FVector::CrossProduct(Right, Normal).GetSafeNormal();
        ClearanceOffset = 0.f;
    }

    /** Capsule remains upright. Its normal extent includes the cylinder's vertical segment on tilted walls. */
    float CapsuleStandOff(float Radius, float HalfHeight, float Gap) const
    {
        return Radius + FMath::Max(0.f, HalfHeight - Radius) * FMath::Abs(Normal.Z) + FMath::Max(1.f, Gap) + ClearanceOffset;
    }

    FQuat FacingRotation() const { return (-Normal).GetSafeNormal2D().Rotation().Quaternion(); }

    FVector2D ToCoordinates(const FVector& Location) const
    {
        const FVector Offset = Location - Origin;
        return FVector2D(FVector::DotProduct(Offset, Right), FVector::DotProduct(Offset, Up));
    }

    FVector ToWorld(const FVector2D& Coordinates, float StandOff) const
    {
        return Origin + Normal * StandOff + Right * Coordinates.X + Up * Coordinates.Y;
    }
};

/** Climbable surfaces: an upright authored rectangle or tilted/faceted WallMesh collision. */
UCLASS(Blueprintable)
class SPYROCLIMBING_API ASpyroClimbSurface : public AActor
{
    GENERATED_BODY()

public:
    ASpyroClimbSurface();
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Spatial query volume, not solid collision. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climbing")
    UBoxComponent* ClimbArea;

    /** Optional separate wall geometry. Has solid collision by default. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climbing")
    UStaticMeshComponent* WallMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climbing")
    UArrowComponent* OutwardArrow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing")
    bool bClimbable = true;

    /** Automatic uses the rectangle for Fit Default Cube, and mesh collision otherwise.
     * Mesh collision supports merged, offset walls, including tilt up to 45 degrees from vertical.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing")
    ESpyroClimbGeometry GeometryMode = ESpyroClimbGeometry::Automatic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing", meta = (ClampMin = "1"))
    float Width = 400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing", meta = (ClampMin = "1"))
    float Height = 600.f;

    /** Sizes/positions the default 100 cm engine cube; turn off for a custom mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing")
    bool bFitDefaultCube = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climbing", meta = (ClampMin = "1"))
    float WallThickness = 20.f;

    /** Refresh after changing dimensions at runtime. */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void RefreshSurface();

    /** Clears the cube's inherited mesh offset/scale for an imported level-aligned mesh.
     * Only the WallMesh relative transform is reset; actor placement and mesh asset are retained.
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Climbing")
    void ResetWallMeshTransform();

    UFUNCTION(BlueprintPure, Category = "Climbing")
    bool UsesMeshCollision() const;

    /** Rectangles require upright unit-scale actors. Mesh mode uses the actual world-space geometry. */
    UFUNCTION(BlueprintPure, Category = "Climbing")
    bool HasSupportedTransform() const;

    FTransform GetGeometryTransform() const;
    bool FindClimbFrame(const FVector& Location, const FVector& Facing, float Reach,
        float MinimumFacingDot, FSpyroClimbFrame& OutFrame) const;
    /** Support width can be smaller than the physical capsule radius to allow side-edge overhang. */
    bool SupportsPosition(const FVector& Position, const FSpyroClimbFrame& Frame,
        float HorizontalSupportRadius, float CapsuleHalfHeight) const;
    /** Adds local clearance for neighbouring facets without changing the physical capsule. */
    bool FitClimbPosition(FVector& InOutPosition, FSpyroClimbFrame& InOutFrame, const FVector& CorrectionDirection,
        float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius) const;
    /** Probes a landing along the departure wall's normal, including other patches of this same mesh. */
    bool FindTransferLanding(const FVector& DesiredPosition, const FVector& SourceNormal, float DepthTolerance,
        float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius,
        FVector& OutPosition, FSpyroClimbFrame& OutFrame) const;
    /** Reprojects one short movement step onto the local mesh, updating the contact frame. */
    FVector ConstrainMeshMove(const FVector& Start, const FVector& DesiredDelta, FSpyroClimbFrame& InOutFrame,
        float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius) const;

    FVector GetOutwardNormal() const;
    FVector GetClimbRight() const;
    FVector GetClimbUp() const;
    FVector2D WorldToClimbCoordinates(const FVector& Location) const;
    FVector ClimbCoordinatesToWorld(const FVector2D& Coordinates, float StandOff) const;
    bool GetUsableExtents(float HorizontalSupportRadius, float CapsuleHalfHeight, FVector2D& OutExtents) const;

private:
    bool ProjectMeshPosition(const FVector& Desired, const FSpyroClimbFrame& PreviousFrame,
        float CapsuleRadius, float CapsuleHalfHeight, float WallGap, float HorizontalSupportRadius,
        FVector& OutPosition, FSpyroClimbFrame& OutFrame) const;
};
