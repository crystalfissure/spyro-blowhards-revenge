#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MMAEditorAnimationLibrary.generated.h"

class UAnimSequence;
class UBlueprint;
class USkeletalMesh;
class AActor;

UCLASS()
class MMAEDITORTOOLS_API UMMAEditorAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "MMA|Animation")
    static bool CopySkeletonNotifies(UAnimSequence* Source, UAnimSequence* Destination);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeBlueprintGraphs(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool CallParameterlessFunction(UObject* Target, FName FunctionName);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool SetSCSComponentAttachSocket(
        UBlueprint* Blueprint,
        FName ComponentVariableName,
        FName SocketName);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddMMAChaseLeashComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("MMA Chase Leash"));

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddMMAHedgeTrimmerBehaviorComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("Hedge Trimmer Behavior"));

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddMMAShieldGuardBehaviorComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("MMA Shield Guard State Machine"));

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddMMAGreenDruidBehaviorComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("Green Druid Behavior"));

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool CompileBlueprint(UBlueprint* Blueprint);

    /**
     * Prevents club-attack enemies from evaluating target-dependent Tick logic
     * while their inherited alert-target array is empty.
     */
    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddClubAttackAlertTargetGuard(UBlueprint* Blueprint);

    /** Makes the shared nearest-alert-player function safe for an empty array. */
    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool AddNearestAlertPlayerEmptyArrayGuard(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAHedgeTrimmerBehavior(
        UBlueprint* Blueprint,
        UAnimSequence* IdleAnimation,
        UAnimSequence* NoticeAnimation,
        UAnimSequence* ChaseAnimation,
        UAnimSequence* AttackAnimation,
        UAnimSequence* ReturnHomeAnimation,
        UAnimSequence* DeathAnimation,
        TSubclassOf<AActor> DefaultDropClass);

    /** Assigns the optional one-frame MMA terminal-death pose. */
    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAEnemyDeathTerminalAnimation(
        UBlueprint* Blueprint,
        UAnimSequence* DeathTerminalAnimation);

    /** Applies data-driven close-melee defaults to the serialized SCS component template. */
    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAEnemyStateMachineSettings(
        UBlueprint* Blueprint,
        const FString& SettingsJson);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAShieldGuardBehavior(
        UBlueprint* Blueprint,
        UAnimSequence* IdleAnimation,
        UAnimSequence* PatrolAnimation,
        UAnimSequence* EnGardeAnimation,
        UAnimSequence* AttackAnimation,
        UAnimSequence* DeathAnimation,
        TSubclassOf<AActor> DefaultDropClass);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAShieldGuardSettings(
        UBlueprint* Blueprint,
        const FString& SettingsJson);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAHedgeTrimmerMesh(
        UBlueprint* Blueprint,
        USkeletalMesh* SkeletalMesh,
        UAnimSequence* PreviewAnimation,
        FVector RelativeLocation,
        FRotator RelativeRotation,
        FVector RelativeScale);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAGreenDruidBehavior(
        UBlueprint* Blueprint,
        UAnimSequence* IdleAnimation,
        UAnimSequence* RaiseAnimation,
        UAnimSequence* LowerAnimation,
        UAnimSequence* DeathAnimation,
        TSubclassOf<AActor> DefaultDropClass);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAGreenDruidPlatform(
        UBlueprint* Blueprint,
        USkeletalMesh* SkeletalMesh,
        UAnimSequence* LiftAnimation,
        FName LiftBoneName);

    /** Merges non-overlapping or more strongly animated raw tracks into a deterministic lift clip. */
    UFUNCTION(BlueprintCallable, Category = "MMA|Animation")
    static bool MergeAnimationTracks(UAnimSequence* Destination, UAnimSequence* AdditionalTracks);

    UFUNCTION(BlueprintPure, Category = "MMA|Animation")
    static FName FindLargestTranslationTrackBone(UAnimSequence* Animation);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeMMAGreenDruidBlueprint(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeMMAHedgeTrimmerBlueprint(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeClassFunctions(UClass* Class);
};
