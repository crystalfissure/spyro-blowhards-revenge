#pragma once

#include "CoreMinimal.h"
#include "PS1IsoGateTypes.generated.h"

USTRUCT(BlueprintType)
struct PS1ISOGATE_API FPS1IsoVerificationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    bool bExists = false;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    bool bAllowedExtension = false;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    bool bHashMatches = false;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    bool bCanPlay = false;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    FString ActualSha256;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    FString BootExecutable;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    TArray<FString> MissingFiles;

    UPROPERTY(BlueprintReadOnly, Category="PS1 ISO Gate")
    FString Message;
};
