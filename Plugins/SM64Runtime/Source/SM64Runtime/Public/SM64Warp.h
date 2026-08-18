#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SM64ActActor.h"
#include "SM64Warp.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64Warp : public ASM64ActActor
{
    GENERATED_BODY()

public:
    ASM64Warp();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* Trigger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    FName WarpId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    ASM64Warp* Destination = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Warp")
    float ReentryDelay = 0.5f;

protected:
    UFUNCTION()
    void OnWarpOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    TMap<TWeakObjectPtr<AActor>, float> LastWarpTimes;
};
