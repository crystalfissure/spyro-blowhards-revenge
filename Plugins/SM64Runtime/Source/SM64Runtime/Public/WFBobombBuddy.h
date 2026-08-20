#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SM64FixedStepActor.h"
#include "WFBobombBuddy.generated.h"

class AWFCannon;
class UAnimSequence;

UENUM(BlueprintType)
enum class EWFBobombBuddyAction : uint8
{
    Idle = 0,
    TurnToTalk = 2,
    Talk = 3
};

UENUM(BlueprintType)
enum class EWFBuddyCannonStatus : uint8
{
    Unopened = 0,
    Opening = 1,
    Opened = 2,
    StopTalking = 3
};

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFBobombBuddy : public ASM64FixedStepActor
{
    GENERATED_BODY()

public:
    AWFBobombBuddy();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ResetForAct_Implementation() override;
    virtual void SimulateSM64Frame_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UCapsuleComponent* InteractionCapsule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    USkeletalMeshComponent* CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Visual")
    USkeletalMesh* DefaultSkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Animation")
    UAnimSequence* IdleAnimation = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SM64|Buddy")
    AWFCannon* LinkedCannon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Buddy")
    int32 FirstDialogId = 47;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Buddy")
    int32 ReadyDialogId = 106;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Buddy")
    bool bAutoInteractOnPawnOverlap = true;

    /** Keeps the unlock flow playable before a dialogue widget is authored. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Buddy")
    bool bAutoCompleteDialogs = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Buddy")
    EWFBuddyCannonStatus CannonStatus = EWFBuddyCannonStatus::Unopened;

    UFUNCTION(BlueprintPure, Category = "SM64|Buddy")
    EWFBobombBuddyAction GetBuddyAction() const;

    UFUNCTION(BlueprintCallable, Category = "SM64|Buddy")
    bool InteractWithBuddy(AActor* Reader);

    UFUNCTION(BlueprintCallable, Category = "SM64|Buddy")
    void CompleteBuddyDialog();

    UFUNCTION(BlueprintCallable, Category = "SM64|Buddy")
    void NotifyCannonOpeningCutsceneComplete();

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Buddy")
    void OnRequestBuddyDialog(int32 DialogId, AActor* Reader);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Buddy")
    void OnRequestCannonCutscene(AWFCannon* CannonActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Buddy")
    void OnBuddyConversationFinished(AActor* Reader);

protected:
    UFUNCTION()
    void OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void EnterBuddyAction(EWFBobombBuddyAction NewAction);
    AWFCannon* ResolveCannon();

    UPROPERTY()
    AActor* ConversationReader = nullptr;
};
