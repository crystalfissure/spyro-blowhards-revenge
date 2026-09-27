#pragma once

#include "CoreMinimal.h"
#include "SpyroClimbingComponent.h"
#include "SpyroClimbingTestObserver.generated.h"

/** Transient listener used by automation to verify the actual Blueprint dispatcher payload. */
UCLASS(Transient, NotBlueprintable)
class USpyroClimbingTestObserver : public UObject
{
    GENERATED_BODY()

public:
    int32 EndCount = 0;
    ESpyroClimbExitReason LastReason = ESpyroClimbExitReason::Cancelled;

    UFUNCTION()
    void OnClimbEnded(ESpyroClimbExitReason Reason)
    {
        ++EndCount;
        LastReason = Reason;
    }
};
