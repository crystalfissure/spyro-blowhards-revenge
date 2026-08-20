#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "SM64Types.h"
#include "SM64CourseManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSM64CountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSM64StarCollected, int32, StarIndex, bool, bIs100CoinStar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSM64ActChanged, int32, NewAct);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSM64CourseGoalReached);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSM64OneUpCollected, int32, SessionOneUpCount);

UCLASS()
class SM64RUNTIME_API USM64ProgressSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Progress")
    TMap<FName, FSM64CourseProgress> CourseProgress;
};

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64CourseManager : public AActor
{
    GENERATED_BODY()

public:
    ASM64CourseManager();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    USM64CourseDefinition* CourseDefinition = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course", meta = (ClampMin = "1", ClampMax = "6"))
    int32 CurrentAct = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    int32 SessionSeed = 0x6405;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Save")
    bool bUseStandaloneSaveFallback = true;

    /** Prefer the loaded Spyro64 SaveGame lineage when travelling from its title/homeworld flow. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Save")
    bool bUseSpyroSaveLineage = true;

    /** Additive Name->int map on 64_SaveData_S1 used for backwards-compatible SM64 keys. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Save")
    FName SpyroProgressMapProperty = TEXT("SM64_CourseProgress");

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Save")
    bool bUsingSpyroSaveLineage = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Save")
    FString StandaloneSaveSlot = TEXT("Spyro64_SM64Progress");

    /** Native fallback used when a course Blueprint does not override the exit event. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    bool bAutoOpenReturnLevel = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Course")
    int32 CoinCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Course")
    int32 RedCoinCount = 0;

    /** Extra lives collected in this loaded course session. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Course")
    int32 SessionOneUpCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Course")
    FSM64CourseProgress Progress;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64CountChanged OnCoinCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64CountChanged OnRedCoinCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64StarCollected OnStarCollected;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64ActChanged OnActChanged;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64CourseGoalReached OnRedCoinGoalReached;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64CourseGoalReached On100CoinGoalReached;

    UPROPERTY(BlueprintAssignable, Category = "SM64|Events")
    FSM64OneUpCollected OnOneUpCollected;

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void SetAct(int32 NewAct, bool bResetTransientState = true);

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void ResetTransientState();

    UFUNCTION(BlueprintCallable, Category = "SM64|Collectibles")
    void AddCoin(int32 Value, bool bRedCoin);

    UFUNCTION(BlueprintCallable, Category = "SM64|Collectibles")
    void CollectStar(int32 StarIndex, bool bIs100CoinStar);

    /** Native SM64 life accounting plus a Blueprint handoff to Spyro's life UI/save system. */
    UFUNCTION(BlueprintCallable, Category = "SM64|Collectibles")
    void AwardOneUp(AActor* Collector);

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void SetCannonUnlocked(bool bUnlocked = true);

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    bool HasMissionStar(int32 StarIndex) const { return Progress.HasMissionStar(StarIndex); }

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void RetryCurrentAct();

    UFUNCTION(BlueprintCallable, Category = "SM64|Save")
    void LoadProgress();

    UFUNCTION(BlueprintCallable, Category = "SM64|Save")
    void SaveProgress();

    UFUNCTION(BlueprintPure, Category = "SM64|Course", meta = (WorldContext = "WorldContextObject"))
    static ASM64CourseManager* FindCourseManager(const UObject* WorldContextObject);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Save")
    void OnRequestSpyroSaveWrite(const FSM64CourseProgress& NewProgress);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course")
    void OnRequestMissionExit(int32 CollectedStarIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course")
    void OnCannonUnlockChanged(bool bUnlocked);

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Collectibles")
    void OnRequestSpyroOneUp(AActor* Collector, int32 NewSessionOneUpCount);

protected:
    virtual void BeginPlay() override;
    void ApplyActGating(bool bResetActors);
    void RevealGoalStar(int32 StarIndex, bool bIs100CoinStar, bool bRedCoinStyle);
    bool TryLoadProgressFromSpyroSave();
    bool TryWriteProgressToSpyroSave();

    bool bRedCoinGoalTriggered = false;
    bool b100CoinGoalTriggered = false;
};
