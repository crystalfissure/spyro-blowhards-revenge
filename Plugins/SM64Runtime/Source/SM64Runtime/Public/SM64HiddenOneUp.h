#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SM64Collectible.h"
#include "SM64HiddenOneUp.generated.h"

/** Hidden 1UP reward shared by trigger-sequence and pole variants. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64HiddenOneUp : public ASM64Collectible
{
    GENERATED_BODY()

public:
    ASM64HiddenOneUp();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    FName TriggerGroup = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP", meta = (ClampMin = "1"))
    int32 RequiredTriggerCount = 2;

    /** Pole rewards home toward the player; ordinary hidden rewards run away. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    bool bHomeTowardPlayer = false;

    /** Authored source position, restored on every act retry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    FVector HomeLocation = FVector::ZeroVector;

    /** Runtime spawn displacement; the pole behavior creates its reward 50 units above its marker. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    FVector RevealOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP", meta = (ClampMin = "1.0"))
    float SimulationHz = 30.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Hidden 1UP")
    int32 TouchedTriggerCount = 0;

    UFUNCTION(BlueprintCallable, Category = "SM64|Hidden 1UP")
    void RegisterHiddenTrigger(int32 TriggerIndex);

    UFUNCTION(BlueprintCallable, Category = "SM64|Hidden 1UP")
    void RevealOneUp();

protected:
    virtual void Tick(float DeltaSeconds) override;
    void SimulateFrame();

    TSet<int32> TouchedTriggerIndices;
    FVector HorizontalDirection = FVector::ForwardVector;
    float AccumulatedSeconds = 0.0f;
    int32 ActionFrame = 0;
    int32 MoveAnglePitchUnits = -0x4000;
    bool bRevealed = false;
    bool bTangible = false;
    bool bRevealOffsetApplied = false;
};

/** One-shot 100x100 SM64 hitbox which advances the nearest reward in its group. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64HiddenOneUpTrigger : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64HiddenOneUpTrigger();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* TriggerBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    FName TriggerGroup = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    int32 TriggerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Hidden 1UP")
    FVector BoxExtent = FVector(100.0f, 100.0f, 50.0f);

protected:
    UFUNCTION()
    void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    bool bTouched = false;
};
