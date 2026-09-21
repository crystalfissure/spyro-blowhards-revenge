#include "SpyroEditorLibrary.h"

#include "ChargeWobbleComponent.h"
#include "GnorcThiefBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Misc/App.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

bool USpyroEditorLibrary::ConfigureGnorcThiefCollisionAndAlert(UBlueprint* Blueprint, USoundAttenuation* AlertAttenuation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !AlertAttenuation) return false;
    auto* Defaults = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!Defaults) return false;
    Blueprint->Modify();
    UGnorcThiefBehaviorComponent* Behavior = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        if (Node) if (auto* Found = Cast<UGnorcThiefBehaviorComponent>(Node->ComponentTemplate)) Behavior = Found;
    if (!Behavior) return false;
    Behavior->Modify();
    Behavior->AlertSoundAttenuation = AlertAttenuation;
    Behavior->AlertVolumeMultiplier = 1.f;
    const float Radius = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    for (bool bSensor : {false, true})
    {
        const FName Name(bSensor ? TEXT("ThiefChargeSensor") : TEXT("ThiefBodyCollision"));
        USCS_Node* Node = Blueprint->SimpleConstructionScript->FindSCSNode(Name);
        if (!Node)
        {
            Node = Blueprint->SimpleConstructionScript->CreateNode(UBoxComponent::StaticClass(), Name);
            Blueprint->SimpleConstructionScript->AddNode(Node);
        }
        auto* Box = Cast<UBoxComponent>(Node->ComponentTemplate);
        if (!Box) return false;
        Box->Modify();
        // Keep player contact outside the capsule's floor sweep. Otherwise a nearby
        // non-walkable pawn can be selected as a base and trigger CharacterMovement::JumpOff.
        const float BodyRadius = Radius + 12.f;
        Box->SetBoxExtent(FVector(BodyRadius + (bSensor ? 8.f : 0.f), BodyRadius + (bSensor ? 8.f : 0.f), HalfHeight - 2.f));
        Box->SetRelativeLocation(FVector::ZeroVector);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        // The physical body is an obstacle, separate from the inherited Pawn capsule
        // used by damage/AI. Player-side pawn filtering must not open a path through it.
        Box->SetCollisionObjectType(ECC_WorldDynamic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4, bSensor ? ECR_Overlap : ECR_Block);
        if (!bSensor) Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        Box->SetGenerateOverlapEvents(bSensor);
        Box->CanCharacterStepUpOn = ECB_No;
        Box->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
        Box->SetCanEverAffectNavigation(false);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool USpyroEditorLibrary::ConfigureGnorcThiefAudio(UBlueprint* Blueprint, const TArray<USoundBase*>& Sounds, USoundAttenuation* Attenuation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || Sounds.Num() != 8 || !Attenuation) return false;
    for (USoundBase* Sound : Sounds) if (!Sound) return false;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        auto* Behavior = Node ? Cast<UGnorcThiefBehaviorComponent>(Node->ComponentTemplate) : nullptr;
        if (!Behavior) continue;
        Blueprint->Modify();
        Behavior->Modify();
        Behavior->OriginalSounds = Sounds;
        Behavior->SoundAttenuation = Attenuation;
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
        return true;
    }
    return false;
}

bool USpyroEditorLibrary::ConfigureGnorcThief(UBlueprint* Blueprint, const TArray<UAnimSequence*>& Animations, USkeletalMesh* FinalMesh)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || Animations.Num() != 6 || !FinalMesh) return false;
    for (UAnimSequence* Animation : Animations) if (!Animation) return false;
    Blueprint->Modify();
    UGnorcThiefBehaviorComponent* Behavior = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        if (Node) if (auto* Found = Cast<UGnorcThiefBehaviorComponent>(Node->ComponentTemplate)) Behavior = Found;
    if (!Behavior)
    {
        USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(UGnorcThiefBehaviorComponent::StaticClass(), TEXT("GnorcThiefBehavior"));
        if (!Node) return false;
        Blueprint->SimpleConstructionScript->AddNode(Node);
        Behavior = Cast<UGnorcThiefBehaviorComponent>(Node->ComponentTemplate);
    }
    Behavior->Modify();
    Behavior->IdleAnimation = Animations[0];
    Behavior->AlertAnimation = Animations[1];
    Behavior->RunAnimation = Animations[2];
    Behavior->AlternateRunAnimation = Animations[3];
    Behavior->HitAnimation = Animations[4];
    Behavior->FinalAnimation = Animations[5];
    Behavior->FinalMesh = FinalMesh;

    // Serialize child overrides so editor details and level gem accounting see 3 HP / 5 gems.
    UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(true);
    if (!Handler) return false;
    UClass* RedGem = LoadClass<AActor>(nullptr, TEXT("/Game/SpyroContent/Global_Assets/Global_Level_Items/Gems/Actors/Child_Actors/Gem_Red_BP.Gem_Red_BP_C"));
    if (!RedGem) return false;
    bool bHealth = false, bDrops = false;
    for (UClass* Parent = Blueprint->ParentClass; Parent; Parent = Parent->GetSuperClass())
    {
        UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Parent);
        if (!Generated || !Generated->SimpleConstructionScript) continue;
        for (USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node || (!Node->ComponentClass->GetName().Contains(TEXT("Damageable_Com")) && !Node->ComponentClass->GetName().Contains(TEXT("Drops_Items")))) continue;
            const FComponentKey Key(Node);
            UActorComponent* Template = Handler->GetOverridenComponentTemplate(Key);
            if (!Template) Template = Handler->CreateOverridenComponentTemplate(Key);
            if (!Template) return false;
            Template->Modify();
            if (auto* HP = FindFProperty<FIntProperty>(Template->GetClass(), TEXT("Hit Points")))
            { HP->SetPropertyValue_InContainer(Template, 3); bHealth = true; }
            if (auto* Items = FindFProperty<FArrayProperty>(Template->GetClass(), TEXT("Items_to_Drop")))
            {
                auto* Inner = CastField<FObjectPropertyBase>(Items->Inner);
                if (!Inner) return false;
                FScriptArrayHelper Array(Items, Items->ContainerPtrToValuePtr<void>(Template));
                Array.EmptyValues(); Array.AddValues(5);
                for (int32 I = 0; I < 5; ++I) Inner->SetObjectPropertyValue(Array.GetRawPtr(I), RedGem);
                bDrops = true;
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return bHealth && bDrops;
}

bool USpyroEditorLibrary::PrepareGnorcThiefChargeTest(AActor* Actor)
{
    ACharacter* Player = Cast<ACharacter>(Actor);
    if (!Player || !Player->GetWorld() || Player->GetWorld()->WorldType != EWorldType::PIE ||
        !Player->GetWorld()->GetMapName().Contains(TEXT("Gnorc_Thief_Test")) || Player->GetClass()->GetName() != TEXT("BP_Spyro_C")) return false;
    auto* State = FindFProperty<FByteProperty>(Player->GetClass(), TEXT("Player_State"));
    if (!State || !State->Enum) return false;
    // Same enum entry used by Base_AICharacter_BP's capsule charge-damage branch.
    const int64 Charging = State->Enum->GetValueByNameString(TEXT("NewEnumerator4"));
    if (Charging == INDEX_NONE) return false;
    State->SetPropertyValue_InContainer(Player, uint8(Charging));
    Player->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel4);
    return true;
}

bool USpyroEditorLibrary::PrepareGnorcThiefTest(AActor* Actor)
{
    if (!Actor || !Actor->GetWorld() || Actor->GetWorld()->WorldType != EWorldType::PIE ||
        !Actor->GetWorld()->GetMapName().Contains(TEXT("Gnorc_Thief_Test")) || !Actor->FindComponentByClass<UGnorcThiefBehaviorComponent>()) return false;
    UGameInstance* Instance = Actor->GetWorld()->GetGameInstance();
    // Offscreen simulation otherwise leaves the editor's audio device active/muted.
    // This fixture is restricted above to the disposable thief PIE test world.
    if (FAudioDevice* Audio = Actor->GetWorld()->GetAudioDeviceRaw())
    {
        Audio->SetTransientMasterVolume(1.f);
        FApp::SetUnfocusedVolumeMultiplier(1.f);
        if (GEngine && GEngine->GetAudioDeviceManager()) GEngine->GetAudioDeviceManager()->SetActiveDevice(Audio->DeviceID);
    }
    auto* LevelName = Instance ? FindFProperty<FStrProperty>(Instance->GetClass(), TEXT("Current_Level_Name")) : nullptr;
    UClass* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/SpyroContent/Global_Assets/Global_SaveData/Individual_Level_SaveData.Individual_Level_SaveData_C"));
    if (!LevelName || !SaveClass) return false;
    const FString Slot(TEXT("GnorcThief_Automation_Only"));
    LevelName->SetPropertyValue_InContainer(Instance, Slot);
    if (!UGameplayStatics::SaveGameToSlot(UGameplayStatics::CreateSaveGameObject(SaveClass), Slot, 0)) return false;
    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetClass()->GetName() == TEXT("Drops_Items_C"))
        {
            if (auto* Ready = FindFProperty<FBoolProperty>(Component->GetClass(), TEXT("Safe to Destroy")))
            {
                Ready->SetPropertyValue_InContainer(Component, true);
                return true;
            }
        }
    }
    return false;
}

bool USpyroEditorLibrary::AddChargeWobbleComponent(
    UBlueprint* Blueprint,
    FName ComponentVariableName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    for (USCS_Node* ExistingNode : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (ExistingNode &&
            (ExistingNode->GetVariableName() == ComponentVariableName ||
             ExistingNode->ComponentClass == UChargeWobbleComponent::StaticClass()))
        {
            return true;
        }
    }
    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UChargeWobbleComponent::StaticClass(), ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }
    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool USpyroEditorLibrary::ConfigureChargeWobble(
    UBlueprint* Blueprint,
    UAnimSequence* RestAnimation,
    UAnimSequence* ReactionAnimation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !RestAnimation || !ReactionAnimation)
    {
        return false;
    }
    bool bConfigured = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UChargeWobbleComponent* Wobble = Node
            ? Cast<UChargeWobbleComponent>(Node->ComponentTemplate) : nullptr;
        if (!Wobble)
        {
            continue;
        }
        Node->Modify();
        Wobble->Modify();
        Wobble->RestAnimation = RestAnimation;
        Wobble->ReactionAnimation = ReactionAnimation;
        bConfigured = true;
    }
    if (bConfigured)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bConfigured;
}

bool USpyroEditorLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}
