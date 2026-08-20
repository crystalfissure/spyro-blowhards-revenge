#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFButterfly.generated.h"

class ASM64Collectible;
class UAnimSequence;

UENUM(BlueprintType)
enum class EWFButterflyState : uint8
{
    Resting,
    FollowPlayer,
    ReturnHome,
    TripletWander,
    Activated
};

/** Exact-art, fixed-step implementation for WF ambient and triplet butterflies. */
UCLASS(Blueprintable)
class SM64RUNTIME_API AWFButterfly : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFButterfly();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USkeletalMeshComponent* CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    USkeletalMesh* DefaultSkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* FlightAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* RestAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Triplet")
    bool bTripletButterfly = false;

    /** NO_BOMBS triplets select exactly one of three butterflies for a 1UP. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Triplet")
    bool bSelectedForOneUp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Triplet")
    TSubclassOf<ASM64Collectible> OneUpClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Triplet")
    float BaseYawDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Butterfly")
    EWFButterflyState ButterflyState = EWFButterflyState::Resting;

protected:
    void EnterState(EWFButterflyState NewState);
    void MoveToward(const FVector& Target, float SpeedPerFrame, float MaxYawDegrees, float MaxPitchDegrees,
        bool bApplyWingBob);
    void SpawnSelectedOneUp();

    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    bool bHomeCaptured = false;
    float VerticalPhase = 0.0f;
    float TripletSpeed = 30.0f;
    bool bSpawnedOneUp = false;
};
