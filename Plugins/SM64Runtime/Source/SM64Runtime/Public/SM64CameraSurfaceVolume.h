#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SM64ActActor.h"
#include "SM64CameraSurfaceVolume.generated.h"

UENUM(BlueprintType)
enum class ESM64CameraSurfaceMode : uint8
{
    BossCamera,
    CameraMiddle,
    Custom
};

/** Volume representation of collision-surface camera metadata. */
UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64CameraSurfaceVolume : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64CameraSurfaceVolume();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* CameraVolume;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Camera Surface")
    FVector BoxExtent = FVector(500.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Camera Surface")
    ESM64CameraSurfaceMode CameraMode = ESM64CameraSurfaceMode::CameraMiddle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Camera Surface")
    FName CameraProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Camera Surface")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Camera Surface")
    float BlendTime = 0.25f;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Camera Surface")
    void OnCameraSurfaceEntered(AActor* PlayerActor, ESM64CameraSurfaceMode RequestedMode,
        FName RequestedProfile, int32 SurfacePriority, float RequestedBlendTime);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Camera Surface")
    void OnCameraSurfaceExited(AActor* PlayerActor, ESM64CameraSurfaceMode PreviousMode,
        FName PreviousProfile, int32 SurfacePriority, float RequestedBlendTime);

protected:
    UFUNCTION()
    void OnCameraVolumeBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnCameraVolumeEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    TSet<AActor*> Occupants;
};
