#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64SignActor.generated.h"

/** Wooden signpost/wall-sign dialogue handoff using the source 150x80 hitbox. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64SignActor : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64SignActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UCapsuleComponent* InteractionCapsule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* ExactCollisionMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    bool bAllowRenderCollisionFallback = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Sign")
    int32 DialogId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Sign")
    float InteractionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Sign")
    float InteractionHeight = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Sign")
    bool bRequireReaderFacingSign = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Sign", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float FacingToleranceDegrees = 75.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Sign")
    bool bDialogueActive = false;

    UFUNCTION(BlueprintPure, Category = "SM64|Sign")
    bool CanReadSign(AActor* Reader) const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Sign")
    bool RequestReadSign(AActor* Reader);

    UFUNCTION(BlueprintCallable, Category = "SM64|Sign")
    void CompleteSignDialog();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Sign")
    void OnReaderEnteredRange(AActor* Reader);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Sign")
    void OnReaderExitedRange(AActor* Reader);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Sign")
    void OnRequestSignDialog(int32 RequestedDialogId, AActor* Reader);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Sign")
    void OnSignDialogCompleted(int32 CompletedDialogId, AActor* Reader);

protected:
    UFUNCTION()
    void OnSignRangeBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnSignRangeEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    UPROPERTY()
    AActor* ActiveReader = nullptr;
};
