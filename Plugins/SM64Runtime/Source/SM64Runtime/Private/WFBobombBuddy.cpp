#include "WFBobombBuddy.h"

#include "Animation/AnimSequence.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "SM64CourseManager.h"
#include "WFCannon.h"

AWFBobombBuddy::AWFBobombBuddy()
{
    ActMask = 0x3C; // Acts 3-6 in WF
    InteractionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractionCapsule"));
    RootComponent = InteractionCapsule;
    InteractionCapsule->SetCapsuleSize(100.0f, 30.0f);
    InteractionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionCapsule->OnComponentBeginOverlap.AddDynamic(
        this, &AWFBobombBuddy::OnInteractionOverlap);

    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(InteractionCapsule);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWFBobombBuddy::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (DefaultSkeletalMesh)
    {
        CharacterMesh->SetSkeletalMesh(DefaultSkeletalMesh);
    }
}

void AWFBobombBuddy::ResetForAct_Implementation()
{
    Super::ResetForAct_Implementation();
    ActionCode = static_cast<int32>(EWFBobombBuddyAction::Idle);
    ActionTimer = 0;
    const ASM64CourseManager* Manager = ASM64CourseManager::FindCourseManager(this);
    CannonStatus = Manager && Manager->Progress.bCannonUnlocked
        ? EWFBuddyCannonStatus::Opened : EWFBuddyCannonStatus::Unopened;
    ConversationReader = nullptr;
    if (IdleAnimation)
    {
        CharacterMesh->PlayAnimation(IdleAnimation, true);
    }
}

EWFBobombBuddyAction AWFBobombBuddy::GetBuddyAction() const
{
    return static_cast<EWFBobombBuddyAction>(ActionCode);
}

void AWFBobombBuddy::EnterBuddyAction(EWFBobombBuddyAction NewAction)
{
    SetActionCode(static_cast<int32>(NewAction));
}

AWFCannon* AWFBobombBuddy::ResolveCannon()
{
    if (LinkedCannon)
    {
        return LinkedCannon;
    }
    if (GetWorld())
    {
        for (TActorIterator<AWFCannon> It(GetWorld()); It; ++It)
        {
            LinkedCannon = *It;
            break;
        }
    }
    return LinkedCannon;
}

bool AWFBobombBuddy::InteractWithBuddy(AActor* Reader)
{
    if (!Reader || GetBuddyAction() != EWFBobombBuddyAction::Idle)
    {
        return false;
    }
    ConversationReader = Reader;
    EnterBuddyAction(EWFBobombBuddyAction::TurnToTalk);
    return true;
}

void AWFBobombBuddy::OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (bAutoInteractOnPawnOverlap && OtherActor && OtherActor->IsA<APawn>())
    {
        InteractWithBuddy(OtherActor);
    }
}

void AWFBobombBuddy::SimulateSM64Frame_Implementation()
{
    const int32 PreviousAction = ActionCode;
    switch (GetBuddyAction())
    {
        case EWFBobombBuddyAction::Idle:
            if (ConversationReader && FVector::Dist(ConversationReader->GetActorLocation(), GetActorLocation()) < 1000.0f)
            {
                TurnSM64YawTowardLocation(ConversationReader->GetActorLocation(), 1.7578125f); // 0x140
            }
            break;

        case EWFBobombBuddyAction::TurnToTalk:
            if (!ConversationReader)
            {
                EnterBuddyAction(EWFBobombBuddyAction::Idle);
                break;
            }
            TurnSM64YawTowardLocation(ConversationReader->GetActorLocation(), 22.5f); // 0x1000
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,
                GetSM64YawTowardLocation(ConversationReader->GetActorLocation()))) < KINDA_SMALL_NUMBER)
            {
                EnterBuddyAction(EWFBobombBuddyAction::Talk);
                OnRequestBuddyDialog(CannonStatus == EWFBuddyCannonStatus::Opened
                    ? ReadyDialogId : FirstDialogId, ConversationReader);
                if (bAutoCompleteDialogs)
                {
                    CompleteBuddyDialog();
                }
            }
            break;

        case EWFBobombBuddyAction::Talk:
            if (CannonStatus == EWFBuddyCannonStatus::Opening)
            {
                if (AWFCannon* Cannon = ResolveCannon())
                {
                    if (Cannon->GetCannonState() == EWFCannonState::OpenIdle)
                    {
                        NotifyCannonOpeningCutsceneComplete();
                    }
                }
            }
            break;
    }
    FinishActionFrame(PreviousAction);
}

void AWFBobombBuddy::CompleteBuddyDialog()
{
    if (GetBuddyAction() != EWFBobombBuddyAction::Talk)
    {
        return;
    }
    if (CannonStatus == EWFBuddyCannonStatus::Unopened)
    {
        AWFCannon* Cannon = ResolveCannon();
        if (Cannon)
        {
            Cannon->UnlockCannon(this);
            CannonStatus = EWFBuddyCannonStatus::Opening;
            OnRequestCannonCutscene(Cannon);
        }
        else
        {
            CannonStatus = EWFBuddyCannonStatus::StopTalking;
            CompleteBuddyDialog();
        }
    }
    else if (CannonStatus == EWFBuddyCannonStatus::Opened)
    {
        CannonStatus = EWFBuddyCannonStatus::StopTalking;
        AActor* Reader = ConversationReader;
        ConversationReader = nullptr;
        EnterBuddyAction(EWFBobombBuddyAction::Idle);
        CannonStatus = EWFBuddyCannonStatus::Opened;
        OnBuddyConversationFinished(Reader);
    }
    else if (CannonStatus == EWFBuddyCannonStatus::StopTalking)
    {
        AActor* Reader = ConversationReader;
        ConversationReader = nullptr;
        EnterBuddyAction(EWFBobombBuddyAction::Idle);
        CannonStatus = EWFBuddyCannonStatus::Opened;
        OnBuddyConversationFinished(Reader);
    }
}

void AWFBobombBuddy::NotifyCannonOpeningCutsceneComplete()
{
    if (GetBuddyAction() == EWFBobombBuddyAction::Talk
        && CannonStatus == EWFBuddyCannonStatus::Opening)
    {
        CannonStatus = EWFBuddyCannonStatus::Opened;
        OnRequestBuddyDialog(ReadyDialogId, ConversationReader);
        if (bAutoCompleteDialogs)
        {
            CompleteBuddyDialog();
        }
    }
}
