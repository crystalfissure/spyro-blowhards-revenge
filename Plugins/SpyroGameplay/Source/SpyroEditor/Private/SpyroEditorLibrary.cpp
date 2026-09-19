#include "SpyroEditorLibrary.h"

#include "ChargeWobbleComponent.h"
#include "GnorcThiefBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

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

bool USpyroEditorLibrary::PrepareGnorcThiefTest(AActor* Actor)
{
    if (!Actor || !Actor->GetWorld() || Actor->GetWorld()->WorldType != EWorldType::PIE ||
        !Actor->GetWorld()->GetMapName().Contains(TEXT("Gnorc_Thief_Test")) || !Actor->FindComponentByClass<UGnorcThiefBehaviorComponent>()) return false;
    UGameInstance* Instance = Actor->GetWorld()->GetGameInstance();
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
