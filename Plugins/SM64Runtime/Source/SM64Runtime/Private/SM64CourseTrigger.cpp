#include "SM64CourseTrigger.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "SM64CourseManager.h"

namespace
{
    int64 GSM64CourseTriggerCheckpointSerial = 0;
}

ASM64CourseTrigger::ASM64CourseTrigger()
{
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASM64CourseTrigger::OnCourseTriggerOverlap);
}

void ASM64CourseTrigger::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TriggerBox->SetBoxExtent(BoxExtent);
    if (CheckpointRespawnTransform.Equals(FTransform::Identity))
    {
        CheckpointRespawnTransform = Transform;
    }
}

void ASM64CourseTrigger::ResetForAct_Implementation()
{
    bCheckpointReached = false;
    CheckpointSerial = 0;
    bProcessingDeath = false;
}

void ASM64CourseTrigger::ActivateCheckpoint(AActor* PlayerActor)
{
    if (TriggerType != ESM64CourseTriggerType::Checkpoint || !PlayerActor)
    {
        return;
    }
    bCheckpointReached = true;
    CheckpointSerial = ++GSM64CourseTriggerCheckpointSerial;
    OnCheckpointReached(PlayerActor, CheckpointRespawnTransform);
}

ASM64CourseTrigger* ASM64CourseTrigger::FindLatestCheckpoint() const
{
    ASM64CourseTrigger* Latest = nullptr;
    int64 LatestSerial = 0;
    if (!GetWorld())
    {
        return nullptr;
    }
    for (TActorIterator<ASM64CourseTrigger> It(GetWorld()); It; ++It)
    {
        if (It->TriggerType == ESM64CourseTriggerType::Checkpoint && It->bCheckpointReached
            && It->CheckpointSerial > LatestSerial)
        {
            Latest = *It;
            LatestSerial = It->CheckpointSerial;
        }
    }
    return Latest;
}

void ASM64CourseTrigger::TriggerDeath(AActor* PlayerActor)
{
    if (TriggerType != ESM64CourseTriggerType::Death || !PlayerActor || bProcessingDeath)
    {
        return;
    }
    bProcessingDeath = true;
    OnDeathTriggered(PlayerActor);
    if (!bAutoRetryActOnDeath)
    {
        bProcessingDeath = false;
        return;
    }

    ASM64CourseTrigger* Checkpoint = bUseLatestCheckpoint ? FindLatestCheckpoint() : nullptr;
    const FTransform CheckpointTransform = Checkpoint ? Checkpoint->CheckpointRespawnTransform : FTransform::Identity;
    if (ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this))
    {
        if (Checkpoint)
        {
            Manager->SetAct(Manager->CurrentAct, true);
            PlayerActor->SetActorTransform(CheckpointTransform, false, nullptr, ETeleportType::TeleportPhysics);
        }
        else
        {
            Manager->RetryCurrentAct();
        }
    }
    bProcessingDeath = false;
}

void ASM64CourseTrigger::OnCourseTriggerOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA<APawn>())
    {
        return;
    }
    if (TriggerType == ESM64CourseTriggerType::Checkpoint)
    {
        ActivateCheckpoint(OtherActor);
    }
    else
    {
        TriggerDeath(OtherActor);
    }
}
