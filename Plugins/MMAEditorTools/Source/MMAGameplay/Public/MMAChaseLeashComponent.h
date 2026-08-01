#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAChaseLeashComponent.generated.h"

/** Limits alert/chase/attack time, then hands control back to the native return-home state. */
UCLASS(ClassGroup = (MMA), meta = (BlueprintSpawnableComponent))
class MMAGAMEPLAY_API UMMAChaseLeashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMAChaseLeashComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Chase", meta = (ClampMin = "0.1"))
    float MaximumChaseSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Chase", meta = (ClampMin = "100.0"))
    float MaximumDistanceFromHome = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Chase", meta = (ClampMin = "25.0"))
    float HomeAcceptanceRadius = 160.0f;

private:
    FVector HomeLocation = FVector::ZeroVector;
    float ChaseElapsedSeconds = 0.0f;
    bool bWasChasing = false;
    bool bForcingReturnHome = false;

    uint8 ReadAIState() const;
    void ClearAlertedPlayers() const;
    bool ChangeAIState(uint8 NewState) const;
};
