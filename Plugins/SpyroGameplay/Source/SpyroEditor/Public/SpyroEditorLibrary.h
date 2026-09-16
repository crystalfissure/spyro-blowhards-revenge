#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpyroEditorLibrary.generated.h"

class UAnimSequence;
class UBlueprint;

UCLASS()
class SPYROEDITOR_API USpyroEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool AddChargeWobbleComponent(
        UBlueprint* Blueprint,
        FName ComponentVariableName = TEXT("ChargeWobble"));

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool ConfigureChargeWobble(
        UBlueprint* Blueprint,
        UAnimSequence* RestAnimation,
        UAnimSequence* ReactionAnimation);

    UFUNCTION(BlueprintCallable, Category = "Spyro|Blueprint")
    static bool CompileBlueprint(UBlueprint* Blueprint);
};
