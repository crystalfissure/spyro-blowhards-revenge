#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMAChaseLeashComponent.generated.h"

class USphereComponent;
class UObject;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMA|Chase", meta = (ClampMin = "1.0"))
    float ForcedReturnSpeed = 200.0f;

private:
    FVector HomeLocation = FVector::ZeroVector;
    float ChaseElapsedSeconds = 0.0f;
    bool bEngagementActive = false;
    bool bWasChasing = false;
    bool bForcingReturnHome = false;
    bool bOriginalCanBecomeAlert = true;
    bool bCanBecomeAlertWasFound = false;
    bool bSpawnPointWasFound = false;
    bool bProximitySpheresSuppressed = false;
    TArray<TWeakObjectPtr<USphereComponent>> SuppressedProximitySpheres;

    uint8 ReadAIState() const;
    void ClearAlertedPlayers() const;
    void SetCanBecomeAlert(bool bEnabled) const;
    void LockInheritedSpawnPointToHome() const;
    UObject* FindSpawnPointOwner() const;
    void ClampToHomeRadius() const;
    void MoveDirectlyTowardHome(float DeltaTime) const;
    void SetProximitySpheresSuppressed(bool bSuppressed);
    bool IsSpyroInsideAlertRadius() const;
    bool ChangeAIState(uint8 NewState) const;
};
