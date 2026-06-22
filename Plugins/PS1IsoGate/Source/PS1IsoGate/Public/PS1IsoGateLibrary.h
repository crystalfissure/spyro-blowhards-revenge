#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PS1IsoGateTypes.h"
#include "PS1IsoGateLibrary.generated.h"

UCLASS()
class PS1ISOGATE_API UPS1IsoGateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult VerifyConfiguredPS1DiscImage();

    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult VerifyPS1DiscImage(const FString& DiscImagePath, const FString& ExpectedSha256, const TArray<FString>& AllowedExtensions, const FString& ExpectedBootExecutable, const TArray<FString>& RequiredDiscFiles);

    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult VerifyPS1DiscImageWithConfiguredRules(const FString& DiscImagePath);

    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static bool ChoosePS1DiscImage(FString& SelectedDiscImagePath);

    UFUNCTION(BlueprintCallable, Category="PS1 ISO Gate")
    static FPS1IsoVerificationResult ChooseAndVerifyConfiguredPS1DiscImage(FString& SelectedDiscImagePath);
};
