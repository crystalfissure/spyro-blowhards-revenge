#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GnorcThiefBehaviorComponent.generated.h"

class ACharacter;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;

UENUM(BlueprintType)
enum class EGnorcThiefState : uint8 { Idle, Alert, Flee, HitRoll, FinalRoll, Dead };

/** Artisans thief adapter: retains inherited navigation, damage resistance and collectible identity. */
UCLASS(ClassGroup=(Spyro), meta=(BlueprintSpawnableComponent))
class SPYROGAMEPLAY_API UGnorcThiefBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGnorcThiefBehaviorComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* IdleAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* AlertAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* RunAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* HitAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") UAnimSequence* FinalAnimation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") USkeletalMesh* FinalMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Animation") float MeshForwardYaw = -90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thief|Behavior", meta=(ClampMin="0.0")) float RecoverySeconds = 0.15f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") EGnorcThiefState State = EGnorcThiefState::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 RemainingHits = 3;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Thief|Debug") int32 GemsSpawned = 0;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    UFUNCTION() void OnAcceptedDamage();
    UFUNCTION() void OnDropperReset();
    void BindContracts();
    void EnterState(EGnorcThiefState NewState);
    void SetMovementHeld(bool bHeld);
    void FaceDirection(const FVector& Direction);
    void DropGemRange(int32 First, int32 Count);
    UPROPERTY(Transient) ACharacter* Character = nullptr;
    UPROPERTY(Transient) USkeletalMeshComponent* Mesh = nullptr;
    UPROPERTY(Transient) USkeletalMesh* MainMesh = nullptr;
    UPROPERTY(Transient) UActorComponent* Damageable = nullptr;
    UPROPERTY(Transient) UActorComponent* WalkingAI = nullptr;
    UPROPERTY(Transient) UActorComponent* Dropper = nullptr;
    float StateElapsed = 0.f;
    float LockedMeshYaw = 0.f;
    bool bFirstTick = true;
    bool bHoldingMovement = false;
    uint8 PreviousMovementMode = 1;
    TSet<int32> ReleasedGemIndices;
};
