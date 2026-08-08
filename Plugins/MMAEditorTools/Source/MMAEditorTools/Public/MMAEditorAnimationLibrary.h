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
    static bool CompileBlueprint(UBlueprint* Blueprint);

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

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static bool ConfigureMMAHedgeTrimmerMesh(
        UBlueprint* Blueprint,
        USkeletalMesh* SkeletalMesh,
        UAnimSequence* PreviewAnimation,
        FVector RelativeLocation,
        FRotator RelativeRotation,
        FVector RelativeScale);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeMMAHedgeTrimmerBlueprint(UBlueprint* Blueprint);

    UFUNCTION(BlueprintCallable, Category = "MMA|Blueprint")
    static FString DescribeClassFunctions(UClass* Class);
};
