#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMAGreenDruidPlatform.generated.h"

class UAnimSequence;
class UBoxComponent;
class USceneComponent;
class USkeletalMeshComponent;

/** Pose-driven moving geometry controlled by one Green Druid. */
UCLASS(Blueprintable, BlueprintType)
class MMAGAMEPLAY_API AMMAGreenDruidPlatform : public AActor
{
    GENERATED_BODY()

public:
    AMMAGreenDruidPlatform();

    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    USceneComponent* SceneRoot;

    /** Render-only source geometry. Collision is supplied by the stable native primitives below. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    USkeletalMeshComponent* PlatformVisual;

    /** Horizontal movable floor used by CharacterMovement's normal movement-base handling. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    UBoxComponent* RideSurface;

    /** Blocks the newly exposed sides between the flat base and the current surface. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    UBoxComponent* ColumnBlocker;

    /** Explicitly linked props are attached here with their world transform preserved. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMA|Green Druid")
    USceneComponent* PayloadRoot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animation")
    UAnimSequence* LiftAnimation = nullptr;

    /** Bone whose vertical motion defines the physical lift. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Animation")
    FName LiftBoneName = NAME_None;

    /** Flat-state center of the ride surface, relative to this actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Collision")
    FVector FlatSurfaceRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Collision")
    FVector RideSurfaceBoxExtent = FVector(150.0f, 150.0f, 12.0f);

    /** Used only when the configured lift bone is unavailable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Green Druid|Collision", meta = (ClampMin = "0.0"))
    float FallbackLiftHeight = 400.0f;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MMA|Green Druid|Payload")
    TArray<AActor*> LinkedPayloadActors;

    UFUNCTION(BlueprintCallable, Category = "MMA|Green Druid")
    bool ClaimController(UObject* NewController);

    UFUNCTION(BlueprintCallable, Category = "MMA|Green Druid")
    void ReleaseController(UObject* ReleasingController);

    UFUNCTION(BlueprintCallable, Category = "MMA|Green Druid")
    void SetLiftAlpha(float NewLiftAlpha);

    UFUNCTION(BlueprintCallable, Category = "MMA|Green Druid")
    void ForceFlat();

    UFUNCTION(BlueprintPure, Category = "MMA|Green Druid")
    bool IsFlat() const;

    UFUNCTION(BlueprintPure, Category = "MMA|Green Druid")
    float GetLiftAlpha() const { return LiftAlpha; }

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> Controller;

    float LiftAlpha = 0.0f;
    float FlatLiftBoneWorldZ = 0.0f;
    bool bHasValidLiftBone = false;
    bool bOwnershipWarningIssued = false;

    void EvaluateVisualPose(float Alpha);
    void UpdatePhysicalGeometry();
    void AttachLinkedPayloads();
};

