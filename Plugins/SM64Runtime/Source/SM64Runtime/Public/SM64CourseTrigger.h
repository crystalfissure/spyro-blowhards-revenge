#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SM64ActActor.h"
#include "SM64CourseTrigger.generated.h"

UENUM(BlueprintType)
enum class ESM64CourseTriggerType : uint8
{
    Checkpoint,
    Death
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64CourseTrigger : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64CourseTrigger();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* TriggerBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course Trigger")
    ESM64CourseTriggerType TriggerType = ESM64CourseTriggerType::Death;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course Trigger")
    FVector BoxExtent = FVector(500.0f, 500.0f, 100.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course Trigger")
    FTransform CheckpointRespawnTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course Trigger")
    bool bAutoRetryActOnDeath = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course Trigger")
    bool bUseLatestCheckpoint = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Course Trigger")
    bool bCheckpointReached = false;

    UFUNCTION(BlueprintCallable, Category = "SM64|Course Trigger")
    void ActivateCheckpoint(AActor* PlayerActor);

    UFUNCTION(BlueprintCallable, Category = "SM64|Course Trigger")
    void TriggerDeath(AActor* PlayerActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course Trigger")
    void OnCheckpointReached(AActor* PlayerActor, FTransform RespawnTransform);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course Trigger")
    void OnDeathTriggered(AActor* PlayerActor);

protected:
    UFUNCTION()
    void OnCourseTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    ASM64CourseTrigger* FindLatestCheckpoint() const;

    int64 CheckpointSerial = 0;
    bool bProcessingDeath = false;
};
