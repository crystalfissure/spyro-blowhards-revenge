#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SM64SessionSubsystem.generated.h"

UCLASS(BlueprintType)
class SM64RUNTIME_API USM64SessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Session")
    bool bHasPendingSelection = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Session")
    FName PendingCourseId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Session")
    int32 PendingAct = 1;

    UFUNCTION(BlueprintCallable, Category = "SM64|Session")
    void SetPendingCourseSelection(FName CourseId, int32 Act);

    UFUNCTION(BlueprintCallable, Category = "SM64|Session")
    bool ConsumePendingCourseSelection(FName CourseId, int32& OutAct);
};
