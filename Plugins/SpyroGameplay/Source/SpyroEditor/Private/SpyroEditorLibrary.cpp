#include "SpyroEditorLibrary.h"

#include "ChargeWobbleComponent.h"
#include "GnorcThiefBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/MemberReference.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/SaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"

namespace SpyroHitReactionEditor
{
FString Key(FString Name)
{
    Name = Name.ToLower();
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name;
}

FProperty* PropertyByKey(UClass* Class, const TCHAR* Name)
{
    if (!Class) return nullptr;
    const FString Wanted = Key(Name);
    for (TFieldIterator<FProperty> It(Class); It; ++It)
        if (Key(It->GetName()) == Wanted) return *It;
    return nullptr;
}

int64 EnumValueByDisplayName(UEnum* Enum, const TCHAR* Name)
{
    if (!Enum) return INDEX_NONE;
    const FString Wanted = Key(Name);
    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        if (Key(Enum->GetNameStringByIndex(Index)) == Wanted ||
            Key(Enum->GetDisplayNameTextByIndex(Index).ToString()) == Wanted)
            return Enum->GetValueByIndex(Index);
    }
    return INDEX_NONE;
}

bool ConfigureDamageable(UActorComponent* Damageable, UBoxComponent* Hitbox)
{
    if (!Damageable || !Hitbox) return false;
    auto* HitboxProperty = CastField<FObjectPropertyBase>(
        PropertyByKey(Damageable->GetClass(), TEXT("Object's Hitbox Component")));
    auto* Resistances = CastField<FArrayProperty>(
        PropertyByKey(Damageable->GetClass(), TEXT("Damage Resistances")));
    auto* Inner = Resistances ? CastField<FByteProperty>(Resistances->Inner) : nullptr;
    if (!HitboxProperty || !Resistances || !Inner ||
        !Hitbox->IsA(HitboxProperty->PropertyClass)) return false;

    HitboxProperty->SetObjectPropertyValue_InContainer(Damageable, Hitbox);
    UEnum* DamageTypes = LoadObject<UEnum>(nullptr,
        TEXT("/Game/SpyroContent/Global_Assets/Global_Characters/Damage_Types.Damage_Types"));
    const int64 Ram = EnumValueByDisplayName(DamageTypes, TEXT("Ram"));
    const int64 Burn = EnumValueByDisplayName(DamageTypes, TEXT("Burn"));
    if (Ram == INDEX_NONE || Burn == INDEX_NONE) return false;

    FScriptArrayHelper Values(Resistances, Resistances->ContainerPtrToValuePtr<void>(Damageable));
    Values.EmptyValues();
    Values.AddValues(2);
    *reinterpret_cast<uint8*>(Values.GetRawPtr(0)) = static_cast<uint8>(Ram);
    *reinterpret_cast<uint8*>(Values.GetRawPtr(1)) = static_cast<uint8>(Burn);
    return true;
}

UK2Node_CallFunction* CallNode(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y)
{
    if (!Graph || !Function) return nullptr;
    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Node = Creator.CreateNode();
    Node->SetFromFunction(Function);
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Creator.Finalize();
    return Node;
}

UK2Node_VariableGet* GetNode(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
    FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
    UK2Node_VariableGet* Node = Creator.CreateNode();
    Node->VariableReference.SetSelfMember(Name);
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Creator.Finalize();
    return Node;
}

UK2Node_VariableSet* SetNode(UEdGraph* Graph, FName Name, bool Value, int32 X, int32 Y)
{
    FGraphNodeCreator<UK2Node_VariableSet> Creator(*Graph);
    UK2Node_VariableSet* Node = Creator.CreateNode();
    Node->VariableReference.SetSelfMember(Name);
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Creator.Finalize();
    if (UEdGraphPin* Pin = Node->FindPin(Name)) Pin->DefaultValue = Value ? TEXT("true") : TEXT("false");
    return Node;
}

UEdGraphPin* ThenPin(UK2Node* Node)
{
    return Node ? Node->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr;
}

bool Connect(const UEdGraphSchema_K2* Schema, UEdGraphPin* A, UEdGraphPin* B)
{
    return Schema && A && B && Schema->TryCreateConnection(A, B);
}
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

bool USpyroEditorLibrary::ConfigureProjectHitReactionBase(UBlueprint* Blueprint)
{
    using namespace SpyroHitReactionEditor;
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !Blueprint->ParentClass ||
        !Blueprint->ParentClass->IsChildOf(AActor::StaticClass())) return false;

    UClass* DamageableClass = LoadClass<UActorComponent>(nullptr,
        TEXT("/Game/SpyroContent/Global_Assets/Global_Components/Damageable_Com.Damageable_Com_C"));
    if (!DamageableClass) return false;

    USCS_Node* MeshNode = nullptr;
    USCS_Node* HitboxNode = nullptr;
    USCS_Node* DamageableNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (!Node) continue;
        if (Node->GetVariableName() == TEXT("Mesh")) MeshNode = Node;
        if (Node->GetVariableName() == TEXT("Hitbox")) HitboxNode = Node;
        if (Node->GetVariableName() == TEXT("Damageable")) DamageableNode = Node;
    }

    Blueprint->Modify();
    if (!MeshNode)
    {
        MeshNode = Blueprint->SimpleConstructionScript->CreateNode(
            USkeletalMeshComponent::StaticClass(), TEXT("Mesh"));
        if (!MeshNode) return false;
        Blueprint->SimpleConstructionScript->AddNode(MeshNode);
    }
    if (!HitboxNode)
    {
        HitboxNode = Blueprint->SimpleConstructionScript->CreateNode(
            UBoxComponent::StaticClass(), TEXT("Hitbox"));
        if (!HitboxNode) return false;
        MeshNode->AddChildNode(HitboxNode);
    }
    if (!DamageableNode)
    {
        DamageableNode = Blueprint->SimpleConstructionScript->CreateNode(
            DamageableClass, TEXT("Damageable"));
        if (!DamageableNode) return false;
        Blueprint->SimpleConstructionScript->AddNode(DamageableNode);
    }

    UBoxComponent* Hitbox = Cast<UBoxComponent>(HitboxNode->ComponentTemplate);
    UActorComponent* Damageable = DamageableNode->ComponentTemplate;
    if (!Hitbox || !Damageable) return false;
    Hitbox->Modify();
    Hitbox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Hitbox->SetGenerateOverlapEvents(true);
    Hitbox->SetBoxExtent(FVector(50.f, 50.f, 100.f));
    Damageable->Modify();
    if (!ConfigureDamageable(Damageable, Hitbox)) return false;

    auto HasVariable = [Blueprint](FName Name)
    {
        for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
            if (Variable.VarName == Name) return true;
        return false;
    };
    FEdGraphPinType AnimationType;
    AnimationType.PinCategory = UEdGraphSchema_K2::PC_Object;
    AnimationType.PinSubCategoryObject = UAnimSequence::StaticClass();
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    if (!HasVariable(TEXT("RestAnimation")) &&
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("RestAnimation"), AnimationType)) return false;
    if (!HasVariable(TEXT("ReactionAnimation")) &&
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ReactionAnimation"), AnimationType)) return false;
    if (!HasVariable(TEXT("bReacting")) &&
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bReacting"), BoolType, TEXT("false"))) return false;
    for (const FName Name : { FName(TEXT("RestAnimation")), FName(TEXT("ReactionAnimation")), FName(TEXT("bReacting")) })
        FBlueprintEditorUtils::SetBlueprintVariableCategory(
            Blueprint, Name, nullptr, FText::FromString(TEXT("Hit Reaction")), true);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return false;

    UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
    auto* DamageableProperty = Blueprint->SkeletonGeneratedClass
        ? CastField<FObjectProperty>(Blueprint->SkeletonGeneratedClass->FindPropertyByName(TEXT("Damageable")))
        : nullptr;
    auto* AttemptedDelegate = CastField<FMulticastDelegateProperty>(
        PropertyByKey(DamageableClass, TEXT("Damage Was Attempted")));
    if (!Graph || !Schema || !DamageableProperty || !AttemptedDelegate) return false;

    static const FString Marker(TEXT("Project-native hit reaction: resisted Ram/Burn -> one-shot animation -> rest pose"));
    Graph->Modify();
    const TArray<UEdGraphNode*> ExistingNodes = Graph->Nodes;
    for (UEdGraphNode* Existing : ExistingNodes)
        if (Existing) Existing->DestroyNode();
    FGraphNodeCreator<UK2Node_ComponentBoundEvent> EventCreator(*Graph);
    UK2Node_ComponentBoundEvent* Event = EventCreator.CreateNode();
    Event->InitializeComponentBoundEventParams(DamageableProperty, AttemptedDelegate);
    Event->NodePosX = 0;
    Event->NodePosY = 0;
    Event->NodeComment = Marker;
    Event->bCommentBubbleVisible = true;
    EventCreator.Finalize();

    UK2Node_VariableGet* ReactingGet = GetNode(Graph, TEXT("bReacting"), 0, 180);
    FGraphNodeCreator<UK2Node_IfThenElse> BranchCreator(*Graph);
    UK2Node_IfThenElse* Branch = BranchCreator.CreateNode();
    Branch->NodePosX = 260;
    Branch->NodePosY = 0;
    BranchCreator.Finalize();
    UK2Node_VariableSet* ReactingTrue = SetNode(Graph, TEXT("bReacting"), true, 500, 70);
    UK2Node_VariableGet* MeshGet = GetNode(Graph, TEXT("Mesh"), 500, 260);
    UK2Node_VariableGet* ReactionGet = GetNode(Graph, TEXT("ReactionAnimation"), 500, 430);
    UK2Node_CallFunction* PlayReaction = CallNode(Graph,
        USkeletalMeshComponent::StaticClass()->FindFunctionByName(TEXT("PlayAnimation")), 760, 70);
    UK2Node_CallFunction* GetLength = CallNode(Graph,
        UAnimSequenceBase::StaticClass()->FindFunctionByName(TEXT("GetPlayLength")), 760, 430);
    UK2Node_CallFunction* Delay = CallNode(Graph,
        UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("Delay")), 1030, 70);
    UK2Node_VariableGet* RestGet = GetNode(Graph, TEXT("RestAnimation"), 1030, 430);
    UK2Node_CallFunction* PlayRest = CallNode(Graph,
        USkeletalMeshComponent::StaticClass()->FindFunctionByName(TEXT("PlayAnimation")), 1290, 70);
    UK2Node_CallFunction* SetPosition = CallNode(Graph,
        USkeletalMeshComponent::StaticClass()->FindFunctionByName(TEXT("SetPosition")), 1550, 70);
    UK2Node_CallFunction* Stop = CallNode(Graph,
        USkeletalMeshComponent::StaticClass()->FindFunctionByName(TEXT("Stop")), 1810, 70);
    UK2Node_VariableSet* ReactingFalse = SetNode(Graph, TEXT("bReacting"), false, 2070, 70);
    if (!Event || !ReactingGet || !Branch || !ReactingTrue || !MeshGet || !ReactionGet ||
        !PlayReaction || !GetLength || !Delay || !RestGet || !PlayRest || !SetPosition ||
        !Stop || !ReactingFalse) return false;

    UEdGraphPin* MeshValue = MeshGet->GetValuePin();
    UEdGraphPin* ReactionValue = ReactionGet->GetValuePin();
    const bool bConnected =
        Connect(Schema, Schema->FindExecutionPin(*Event, EGPD_Output), Branch->GetExecPin()) &&
        Connect(Schema, ReactingGet->GetValuePin(), Branch->GetConditionPin()) &&
        Connect(Schema, Branch->GetElsePin(), ReactingTrue->GetExecPin()) &&
        Connect(Schema, ThenPin(ReactingTrue), PlayReaction->GetExecPin()) &&
        Connect(Schema, MeshValue, PlayReaction->FindPin(UEdGraphSchema_K2::PN_Self)) &&
        Connect(Schema, ReactionValue, PlayReaction->FindPin(TEXT("NewAnimToPlay"))) &&
        Connect(Schema, ReactionValue, GetLength->FindPin(UEdGraphSchema_K2::PN_Self)) &&
        Connect(Schema, ThenPin(PlayReaction), GetLength->GetExecPin()) &&
        Connect(Schema, GetLength->GetReturnValuePin(), Delay->FindPin(TEXT("Duration"))) &&
        Connect(Schema, ThenPin(GetLength), Delay->GetExecPin()) &&
        Connect(Schema, ThenPin(Delay), PlayRest->GetExecPin()) &&
        Connect(Schema, MeshValue, PlayRest->FindPin(UEdGraphSchema_K2::PN_Self)) &&
        Connect(Schema, RestGet->GetValuePin(), PlayRest->FindPin(TEXT("NewAnimToPlay"))) &&
        Connect(Schema, ThenPin(PlayRest), SetPosition->GetExecPin()) &&
        Connect(Schema, MeshValue, SetPosition->FindPin(UEdGraphSchema_K2::PN_Self)) &&
        Connect(Schema, ThenPin(SetPosition), Stop->GetExecPin()) &&
        Connect(Schema, MeshValue, Stop->FindPin(UEdGraphSchema_K2::PN_Self)) &&
        Connect(Schema, ThenPin(Stop), ReactingFalse->GetExecPin());
    if (!bConnected) return false;
    if (UEdGraphPin* Loop = PlayReaction->FindPin(TEXT("bLooping"))) Loop->DefaultValue = TEXT("false");
    if (UEdGraphPin* Loop = PlayRest->FindPin(TEXT("bLooping"))) Loop->DefaultValue = TEXT("false");
    if (UEdGraphPin* Position = SetPosition->FindPin(TEXT("InPos"))) Position->DefaultValue = TEXT("0.0");
    if (UEdGraphPin* Notify = SetPosition->FindPin(TEXT("bFireNotifies"))) Notify->DefaultValue = TEXT("false");

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint->Status != BS_Error;
}

bool USpyroEditorLibrary::MigrateLanternToProjectHitReactionBase(
    UBlueprint* Blueprint,
    UBlueprint* BaseBlueprint,
    UAnimSequence* RestAnimation,
    UAnimSequence* ReactionAnimation)
{
    using namespace SpyroHitReactionEditor;
    if (!Blueprint || !BaseBlueprint || !BaseBlueprint->GeneratedClass ||
        !RestAnimation || !ReactionAnimation || !Blueprint->GeneratedClass ||
        !BaseBlueprint->GeneratedClass->IsChildOf(AActor::StaticClass())) return false;

    ASkeletalMeshActor* OldCDO = Cast<ASkeletalMeshActor>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* OldMeshComponent = OldCDO ? OldCDO->GetSkeletalMeshComponent() : nullptr;
    USkeletalMesh* Mesh = OldMeshComponent ? OldMeshComponent->SkeletalMesh : nullptr;
    if (!Mesh) return false;
    TArray<UMaterialInterface*> Materials;
    for (int32 Index = 0; Index < OldMeshComponent->GetNumMaterials(); ++Index)
        Materials.Add(OldMeshComponent->GetMaterial(Index));

    Blueprint->Modify();
    TArray<USCS_Node*> Remove;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        if (Node && (Node->ComponentClass == UChargeWobbleComponent::StaticClass() ||
            Node->GetVariableName() == TEXT("ChargeWobble"))) Remove.Add(Node);
    for (USCS_Node* Node : Remove) Blueprint->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);

    Blueprint->ParentClass = BaseBlueprint->GeneratedClass;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error || !Blueprint->GeneratedClass) return false;

    UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(true);
    USkeletalMeshComponent* MeshOverride = nullptr;
    UBoxComponent* HitboxOverride = nullptr;
    UActorComponent* DamageableOverride = nullptr;
    if (!Handler || !BaseBlueprint->SimpleConstructionScript) return false;
    for (USCS_Node* Node : BaseBlueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (!Node) continue;
        UActorComponent* Override = Handler->GetOverridenComponentTemplate(FComponentKey(Node));
        if (!Override) Override = Handler->CreateOverridenComponentTemplate(FComponentKey(Node));
        if (Node->GetVariableName() == TEXT("Mesh")) MeshOverride = Cast<USkeletalMeshComponent>(Override);
        if (Node->GetVariableName() == TEXT("Hitbox")) HitboxOverride = Cast<UBoxComponent>(Override);
        if (Node->GetVariableName() == TEXT("Damageable")) DamageableOverride = Override;
    }
    if (!MeshOverride || !HitboxOverride || !DamageableOverride) return false;
    MeshOverride->Modify();
    MeshOverride->SetSkeletalMesh(Mesh);
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
        MeshOverride->SetMaterial(Index, Materials[Index]);
    MeshOverride->SetCollisionEnabled(OldMeshComponent->GetCollisionEnabled());
    MeshOverride->SetCastShadow(OldMeshComponent->CastShadow);
    MeshOverride->SetRelativeTransform(OldMeshComponent->GetRelativeTransform());
    MeshOverride->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    MeshOverride->AnimationData.AnimToPlay = RestAnimation;
    MeshOverride->AnimationData.bSavedLooping = false;
    MeshOverride->AnimationData.bSavedPlaying = false;
    MeshOverride->AnimationData.SavedPosition = 0.f;
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    HitboxOverride->Modify();
    HitboxOverride->SetRelativeLocation(Bounds.Origin);
    HitboxOverride->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(1.f)));
    HitboxOverride->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    HitboxOverride->SetGenerateOverlapEvents(true);
    DamageableOverride->Modify();
    if (!ConfigureDamageable(DamageableOverride, HitboxOverride)) return false;

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error || !Blueprint->GeneratedClass) return false;

    AActor* NewCDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (!NewCDO) return false;
    NewCDO->Modify();

    auto* RestProperty = CastField<FObjectPropertyBase>(
        Blueprint->GeneratedClass->FindPropertyByName(TEXT("RestAnimation")));
    auto* ReactionProperty = CastField<FObjectPropertyBase>(
        Blueprint->GeneratedClass->FindPropertyByName(TEXT("ReactionAnimation")));
    auto* ReactingProperty = CastField<FBoolProperty>(
        Blueprint->GeneratedClass->FindPropertyByName(TEXT("bReacting")));
    if (!RestProperty || !ReactionProperty || !ReactingProperty) return false;
    RestProperty->SetObjectPropertyValue_InContainer(NewCDO, RestAnimation);
    ReactionProperty->SetObjectPropertyValue_InContainer(NewCDO, ReactionAnimation);
    ReactingProperty->SetPropertyValue_InContainer(NewCDO, false);
    Blueprint->MarkPackageDirty();
    return true;
}
