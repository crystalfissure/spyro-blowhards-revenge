#include "SM64SessionSubsystem.h"

void USM64SessionSubsystem::SetPendingCourseSelection(FName CourseId, int32 Act)
{
    PendingCourseId = CourseId;
    PendingAct = FMath::Clamp(Act, 1, 6);
    bHasPendingSelection = true;
}

bool USM64SessionSubsystem::ConsumePendingCourseSelection(FName CourseId, int32& OutAct)
{
    if (!bHasPendingSelection || PendingCourseId != CourseId)
    {
        return false;
    }
    OutAct = PendingAct;
    bHasPendingSelection = false;
    PendingCourseId = NAME_None;
    PendingAct = 1;
    return true;
}
