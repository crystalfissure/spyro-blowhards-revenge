#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SM64Types.h"
#include "SM64StarSelectWidget.generated.h"

class ASM64CoursePortal;
class SWidget;

UCLASS(Blueprintable)
class SM64RUNTIME_API USM64StarSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "SM64|Course")
    ASM64CoursePortal* CoursePortal = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SM64|Course")
    USM64CourseDefinition* CourseDefinition = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SM64|Course")
    FSM64CourseProgress CourseProgress;

    UPROPERTY(BlueprintReadOnly, Category = "SM64|Course")
    int32 SuggestedAct = 1;

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void Configure(
        ASM64CoursePortal* InPortal,
        USM64CourseDefinition* InDefinition,
        const FSM64CourseProgress& InProgress,
        int32 InSuggestedAct);

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    bool ChooseAct(int32 Act);

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    bool IsActUnlocked(int32 Act) const;

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    bool IsMissionCollected(int32 StarIndex) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course")
    void OnCourseDataConfigured();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    FReply HandleActClicked(int32 Act);
};
