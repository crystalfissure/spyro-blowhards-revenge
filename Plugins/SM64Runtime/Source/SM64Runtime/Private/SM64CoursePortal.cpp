#include "SM64CoursePortal.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SM64CourseManager.h"
#include "SM64SessionSubsystem.h"
#include "SM64StarSelectWidget.h"

ASM64CoursePortal::ASM64CoursePortal()
{
    PrimaryActorTick.bCanEverTick = false;
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    RootComponent = Trigger;
    Trigger->SetBoxExtent(FVector(140.0f, 140.0f, 180.0f));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASM64CoursePortal::OnPortalOverlap);
}

void ASM64CoursePortal::OnPortalOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (APawn* Pawn = Cast<APawn>(OtherActor))
    {
        OpenStarSelect(Pawn);
    }
}

FSM64CourseProgress ASM64CoursePortal::GetCourseProgress() const
{
    FSM64CourseProgress Result;
    const USM64ProgressSaveGame* Save = Cast<USM64ProgressSaveGame>(
        UGameplayStatics::LoadGameFromSlot(StandaloneSaveSlot, 0));
    if (!Save)
    {
        return Result;
    }
    const FName CourseId = CourseDefinition ? CourseDefinition->CourseId : FName(TEXT("WF"));
    if (const FSM64CourseProgress* Found = Save->CourseProgress.Find(CourseId))
    {
        Result = *Found;
    }
    return Result;
}

bool ASM64CoursePortal::IsActUnlocked(int32 Act) const
{
    if (Act < 1 || Act > 6)
    {
        return false;
    }
    if (!bRequireSequentialUnlock || Act == 1)
    {
        return true;
    }
    return GetCourseProgress().HasMissionStar(Act - 2);
}

int32 ASM64CoursePortal::GetNextUnlockedAct() const
{
    int32 Result = 1;
    for (int32 Act = 2; Act <= 6; ++Act)
    {
        if (!IsActUnlocked(Act))
        {
            break;
        }
        Result = Act;
    }
    return Result;
}

void ASM64CoursePortal::OpenStarSelect(APawn* PlayerPawn)
{
    const FSM64CourseProgress Progress = GetCourseProgress();
    const int32 SuggestedAct = GetNextUnlockedAct();
    OnStarSelectOpened(PlayerPawn, Progress, SuggestedAct);

    if (ActiveWidget)
    {
        return;
    }
    APlayerController* Controller = PlayerPawn ? Cast<APlayerController>(PlayerPawn->GetController()) : nullptr;
    TSubclassOf<USM64StarSelectWidget> WidgetClass = StarSelectWidgetClass;
    if (!WidgetClass)
    {
        WidgetClass = USM64StarSelectWidget::StaticClass();
    }
    ActiveWidget = Controller
        ? CreateWidget<USM64StarSelectWidget>(Controller, WidgetClass)
        : nullptr;
    if (ActiveWidget)
    {
        ActiveWidget->Configure(this, CourseDefinition, Progress, SuggestedAct);
        ActiveWidget->AddToViewport();
        Controller->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(ActiveWidget->TakeWidget());
        Controller->SetInputMode(InputMode);
    }
}

bool ASM64CoursePortal::SelectMissionAndTravel(int32 Act)
{
    if (!IsActUnlocked(Act) || TargetLevel.IsNone())
    {
        return false;
    }
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (USM64SessionSubsystem* Session = GameInstance->GetSubsystem<USM64SessionSubsystem>())
        {
            const FName CourseId = CourseDefinition ? CourseDefinition->CourseId : FName(TEXT("WF"));
            Session->SetPendingCourseSelection(CourseId, Act);
        }
    }
    if (ActiveWidget)
    {
        ActiveWidget->RemoveFromParent();
        ActiveWidget = nullptr;
    }
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->bShowMouseCursor = false;
        Controller->SetInputMode(FInputModeGameOnly());
    }
    UGameplayStatics::OpenLevel(this, TargetLevel);
    return true;
}
