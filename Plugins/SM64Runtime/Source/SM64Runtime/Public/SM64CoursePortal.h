#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "SM64Types.h"
#include "SM64CoursePortal.generated.h"

class APawn;
class USM64StarSelectWidget;

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64CoursePortal : public AActor
{
    GENERATED_BODY()

public:
    ASM64CoursePortal();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UBoxComponent* Trigger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    USM64CourseDefinition* CourseDefinition = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    FName TargetLevel = TEXT("/Game/Spyro64/Levels/05_Level5");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    TSubclassOf<USM64StarSelectWidget> StarSelectWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Course")
    bool bRequireSequentialUnlock = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Save")
    FString StandaloneSaveSlot = TEXT("Spyro64_SM64Progress");

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    void OpenStarSelect(APawn* PlayerPawn);

    UFUNCTION(BlueprintCallable, Category = "SM64|Course")
    bool SelectMissionAndTravel(int32 Act);

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    FSM64CourseProgress GetCourseProgress() const;

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    bool IsActUnlocked(int32 Act) const;

    UFUNCTION(BlueprintPure, Category = "SM64|Course")
    int32 GetNextUnlockedAct() const;

    UFUNCTION(BlueprintImplementableEvent, Category = "SM64|Course")
    void OnStarSelectOpened(APawn* PlayerPawn, const FSM64CourseProgress& CourseProgress, int32 SuggestedAct);

protected:
    UFUNCTION()
    void OnPortalOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UPROPERTY(Transient)
    USM64StarSelectWidget* ActiveWidget = nullptr;
};
