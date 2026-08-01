#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MMAEditorAnimationLibrary.generated.h"

class UAnimSequence;
class UBlueprint;

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
};
