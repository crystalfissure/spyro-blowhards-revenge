#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "SM64PowerStar.generated.h"

UENUM(BlueprintType)
enum class ESM64PowerStarState : uint8
{
    Hidden,
    SpawnPause,
    FlyToHome,
    Settle,
    Available,
    CollectionCutscene,
    Collected
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64PowerStar : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    ASM64PowerStar();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USphereComponent* Trigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    int32 StarIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    bool b100CoinStar = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    bool bNoExit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    bool bStartAvailable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    bool bWaitForBlueprintCollectionCutscene = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star|Collected Presentation")
    bool bShowCollectedStaticStars = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star|Collected Presentation")
    float CollectedOpacity = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star|Collected Presentation")
    FName OpacityParameter = TEXT("Opacity");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star|Collected Presentation")
    FName TintParameter = TEXT("Tint");

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Star|Collected Presentation")
    bool bCollectedPresentation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Star")
    FVector HomeLocation;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Star")
    AActor* Collector = nullptr;

    UFUNCTION(BlueprintPure, Category = "SM64|Star")
    ESM64PowerStarState GetPowerStarState() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Star")
    void BeginSpawnSequence(FVector SpawnLocation, FVector TargetHomeLocation, bool bRedCoinStyleCutscene);

    UFUNCTION(BlueprintCallable, Category = "SM64|Star")
    void RevealImmediately(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "SM64|Star")
    void CompleteCollectionCutscene();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star")
    void OnStarSpawnCutsceneStarted(bool bRedCoinStyle);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star")
    void OnStarSpawnSparkle(FVector WorldLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star")
    void OnStarBecameCollectible();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star")
    void OnStarSpawnCutsceneFinished();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Star")
    void OnRequestStarCollectionCutscene(AActor* CollectingActor, int32 CollectedStarIndex, bool bNonEjecting);

protected:
    UFUNCTION()
    void OnStarOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void EnterStarState(ESM64PowerStarState NewState);
    void SetStarCollectible(bool bVisible, bool bCollectible);
    void ApplyCollectedPresentation(bool bCollected);

    FVector SpawnStartLocation = FVector::ZeroVector;
    FVector LinearFlightBase = FVector::ZeroVector;
    FVector LinearVelocityPerFrame = FVector::ZeroVector;
    bool bRedCoinSpawnStyle = false;
    bool bHomeCaptured = false;
    bool bSpawnSequenceActive = false;
};
