#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SpyroEnemyEvents.generated.h"

/** Optional presentation/encounter notifications; the native simulation owns state. */
UENUM(BlueprintType)
enum class ESpyroEnemySignal : uint8
{
    AttackCommitted,
    StageChanged,
    GuardsReleased,
    RecoveryStarted,
    RouteChanged,
    ResetCompleted
};

/** Detail is the attack clip, stage, recovery count or route node for that signal. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSpyroEnemySignalEvent, UActorComponent*, Behavior,
    ESpyroEnemySignal, Signal, int32, Detail);

/** Dispatch after a native update, never from the middle of collision/state math. */
struct FSpyroEnemyEventQueue
{
    struct FNotice { ESpyroEnemySignal Signal; int32 Detail; };
    void Add(ESpyroEnemySignal Signal, int32 Detail = 0) { Pending.Add({Signal, Detail}); }
    void Reset() { ++Generation; Pending.Reset(); }
    int32 Num() const { return Pending.Num(); }

    template<typename FNotify>
    void Dispatch(UActorComponent* Behavior, FNotify&& Notify)
    {
        if (bDispatching || Pending.Num() == 0) return;
        TGuardValue<bool> Guard(bDispatching, true);
        TWeakObjectPtr<UActorComponent> Subject(Behavior);
        TWeakObjectPtr<AActor> Owner(Behavior ? Behavior->GetOwner() : nullptr);
        const uint32 BatchGeneration = Generation;
        TArray<FNotice> Batch = MoveTemp(Pending);
        for (const FNotice& Notice : Batch)
        {
            // A Blueprint listener may reset or destroy this actor. Old notices
            // must not continue into the new lifecycle or dereference that actor.
            if (Generation != BatchGeneration || !Subject.IsValid() || !Owner.IsValid() ||
                Owner->IsActorBeingDestroyed()) break;
            Notify(Notice.Signal, Notice.Detail);
        }
    }
    void Flush(UActorComponent* Behavior, FSpyroEnemySignalEvent& Event)
    {
        Dispatch(Behavior, [&](ESpyroEnemySignal Signal, int32 Detail)
        { Event.Broadcast(Behavior, Signal, Detail); });
    }
private:
    TArray<FNotice> Pending;
    uint32 Generation = 0;
    bool bDispatching = false;
};
