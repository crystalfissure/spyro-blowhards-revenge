#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64Types.h"
#include "SM64MovingPlatformBase.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64MovingPlatformBase : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64MovingPlatformBase();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetCurrentAct(int32 NewAct) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* PlatformMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* CollisionMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* RiderSensor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    UStaticMesh* DefaultMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Collision")
    UStaticMesh* DefaultCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    ESM64PlatformMotion Motion = ESM64PlatformMotion::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    float SimulationHz = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    int32 SessionSeed = 0x6405;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    int32 InitialPhaseFrames = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    float SpeedPerFrame = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    float TravelDistance = 510.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    float RotationDegreesPerFrame = 0.703125f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Motion")
    FVector MotionDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Rider")
    FVector RiderSensorExtent = FVector(160.0f, 160.0f, 100.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Rider")
    FVector RiderSensorOffset = FVector(0.0f, 0.0f, 90.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Rider")
    bool bManualRiderConveyance = true;

    UFUNCTION(BlueprintCallable, Category = "SM64|Motion")
    virtual void ResetMotion();

    UFUNCTION(BlueprintPure, Category = "SM64|Motion")
    int64 GetSimulationFrame() const { return SimulationFrame; }

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Motion")
    void OnSM64SimulationStep(int64 FrameNumber);

protected:
    virtual void BeginPlay() override;
    virtual void StepSimulation();
    virtual void ApplyPlatformTransform(const FTransform& NewTransform);
    virtual void SetMotionAction(int32 NewAction);

    UFUNCTION()
    void OnRiderBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnRiderEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    FTransform HomeTransform;
    bool bHomeTransformInitialized = false;
    double StepAccumulator = 0.0;
    int64 SimulationFrame = 0;
    int32 MotionAction = 0;
    int32 ActionTimer = 0;
    float VerticalVelocity = 0.0f;
    float PitchVelocity = 0.0f;
    float RollVelocity = 0.0f;
    float RollAcceleration = 0.0f;
    float TumblingFloorHeight = 0.0f;
    float CurrentForwardSpeed = 0.0f;
    bool bInsideSimulationStep = false;
    TArray<TWeakObjectPtr<AActor>> Riders;
    FRandomStream RandomStream;
};
