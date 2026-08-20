#include "SM64StarSelectWidget.h"

#include "SM64CoursePortal.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void USM64StarSelectWidget::Configure(
    ASM64CoursePortal* InPortal,
    USM64CourseDefinition* InDefinition,
    const FSM64CourseProgress& InProgress,
    int32 InSuggestedAct)
{
    CoursePortal = InPortal;
    CourseDefinition = InDefinition;
    CourseProgress = InProgress;
    SuggestedAct = FMath::Clamp(InSuggestedAct, 1, 6);
    OnCourseDataConfigured();
}

bool USM64StarSelectWidget::ChooseAct(int32 Act)
{
    return CoursePortal && CoursePortal->SelectMissionAndTravel(Act);
}

bool USM64StarSelectWidget::IsActUnlocked(int32 Act) const
{
    return CoursePortal && CoursePortal->IsActUnlocked(Act);
}

bool USM64StarSelectWidget::IsMissionCollected(int32 StarIndex) const
{
    return CourseProgress.HasMissionStar(StarIndex);
}

TSharedRef<SWidget> USM64StarSelectWidget::RebuildWidget()
{
    TSharedRef<SVerticalBox> MissionList = SNew(SVerticalBox);
    MissionList->AddSlot().AutoHeight().Padding(12.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("WHOMP'S FORTRESS")))
        .Justification(ETextJustify::Center)
    ];

    for (int32 Act = 1; Act <= 6; ++Act)
    {
        FString MissionName = FString::Printf(TEXT("Mission %d"), Act);
        if (CourseDefinition && CourseDefinition->Missions.IsValidIndex(Act - 1))
        {
            MissionName = CourseDefinition->Missions[Act - 1].DisplayName.ToString();
        }
        if (IsMissionCollected(Act - 1))
        {
            MissionName = TEXT("★ ") + MissionName;
        }
        MissionList->AddSlot().AutoHeight().Padding(8.0f, 4.0f)
        [
            SNew(SButton)
            .IsEnabled(IsActUnlocked(Act))
            .OnClicked_UObject(this, &USM64StarSelectWidget::HandleActClicked, Act)
            [
                SNew(STextBlock).Text(FText::FromString(MissionName))
            ]
        ];
    }

    return SNew(SBorder)
        .Padding(24.0f)
        [
            SNew(SBox)
            .WidthOverride(520.0f)
            [
                MissionList
            ]
        ];
}

FReply USM64StarSelectWidget::HandleActClicked(int32 Act)
{
    ChooseAct(Act);
    return FReply::Handled();
}
