#include "MMAEditorAnimationLibrary.h"

#include "MMAChaseLeashComponent.h"
#include "MMAHedgeTrimmerBehaviorComponent.h"
#include "MMAGreenDruidBehaviorComponent.h"
#include "MMAGreenDruidPlatform.h"
#include "MMAShieldGuardBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Particles/ParticleSystem.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Sound/SoundBase.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool UMMAEditorAnimationLibrary::CopySkeletonNotifies(
    UAnimSequence* Source,
    UAnimSequence* Destination)
{
    if (!Source || !Destination)
    {
        return false;
    }

    Destination->Modify();
    Destination->Notifies.Reset();
#if WITH_EDITOR
    Destination->InitializeNotifyTrack();
#endif

    for (const FAnimNotifyEvent& SourceEvent : Source->Notifies)
    {
        if (SourceEvent.Notify || SourceEvent.NotifyStateClass || SourceEvent.NotifyName.IsNone())
        {
            continue;
        }

        FAnimNotifyEvent DestinationEvent;
        DestinationEvent.NotifyName = SourceEvent.NotifyName;
        DestinationEvent.TriggerWeightThreshold = SourceEvent.TriggerWeightThreshold;
        DestinationEvent.MontageTickType = SourceEvent.MontageTickType;
        DestinationEvent.NotifyTriggerChance = SourceEvent.NotifyTriggerChance;
        DestinationEvent.NotifyFilterType = SourceEvent.NotifyFilterType;
        DestinationEvent.NotifyFilterLOD = SourceEvent.NotifyFilterLOD;
        DestinationEvent.bTriggerOnDedicatedServer = SourceEvent.bTriggerOnDedicatedServer;
        DestinationEvent.bTriggerOnFollower = SourceEvent.bTriggerOnFollower;
        DestinationEvent.TrackIndex = 0;
        DestinationEvent.Link(
            Destination,
            FMath::Clamp(SourceEvent.GetTriggerTime(), 0.0f, Destination->SequenceLength));
        Destination->Notifies.Add(DestinationEvent);
    }

    Destination->SortNotifies();
    Destination->RefreshCacheData();
    Destination->MarkPackageDirty();
    Destination->PostEditChange();
    return Destination->Notifies.Num() > 0;
}

FString UMMAEditorAnimationLibrary::DescribeBlueprintGraphs(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TEXT("{}");
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<TSharedPtr<FJsonValue>> GraphValues;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }
        TSharedRef<FJsonObject> GraphObject = MakeShared<FJsonObject>();
        GraphObject->SetStringField(TEXT("name"), Graph->GetName());
        GraphObject->SetStringField(TEXT("path"), Graph->GetPathName());
        TArray<TSharedPtr<FJsonValue>> NodeValues;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }
            TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
            NodeObject->SetStringField(TEXT("name"), Node->GetName());
            NodeObject->SetStringField(TEXT("class"), Node->GetClass()->GetName());
            NodeObject->SetStringField(
                TEXT("title"),
                Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            NodeObject->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
            NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
            NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
            TArray<TSharedPtr<FJsonValue>> PinValues;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin)
                {
                    continue;
                }
                TSharedRef<FJsonObject> PinObject = MakeShared<FJsonObject>();
                PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
                PinObject->SetStringField(
                    TEXT("direction"),
                    Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
                PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
                PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
                PinObject->SetStringField(TEXT("default"), Pin->DefaultValue);
                if (Pin->DefaultObject)
                {
                    PinObject->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
                }
                TArray<TSharedPtr<FJsonValue>> LinkValues;
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (!LinkedPin || !LinkedPin->GetOwningNode())
                    {
                        continue;
                    }
                    TSharedRef<FJsonObject> LinkObject = MakeShared<FJsonObject>();
                    LinkObject->SetStringField(
                        TEXT("node_guid"),
                        LinkedPin->GetOwningNode()->NodeGuid.ToString());
                    LinkObject->SetStringField(TEXT("node"), LinkedPin->GetOwningNode()->GetName());
                    LinkObject->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
                    LinkValues.Add(MakeShared<FJsonValueObject>(LinkObject));
                }
                PinObject->SetArrayField(TEXT("links"), LinkValues);
                PinValues.Add(MakeShared<FJsonValueObject>(PinObject));
            }
            NodeObject->SetArrayField(TEXT("pins"), PinValues);
            NodeValues.Add(MakeShared<FJsonValueObject>(NodeObject));
        }
        GraphObject->SetArrayField(TEXT("nodes"), NodeValues);
        GraphValues.Add(MakeShared<FJsonValueObject>(GraphObject));
    }
    Root->SetArrayField(TEXT("graphs"), GraphValues);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

bool UMMAEditorAnimationLibrary::CallParameterlessFunction(
    UObject* Target,
    FName FunctionName)
{
    if (!Target)
    {
        return false;
    }
    UFunction* Function = Target->FindFunction(FunctionName);
    if (!Function || Function->ParmsSize != 0)
    {
        return false;
    }
    Target->ProcessEvent(Function, nullptr);
    return true;
}

bool UMMAEditorAnimationLibrary::SetSCSComponentAttachSocket(
    UBlueprint* Blueprint,
    FName ComponentVariableName,
    FName SocketName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName() == ComponentVariableName)
        {
            Blueprint->Modify();
            Node->Modify();
            Node->AttachToName = SocketName;
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            Blueprint->MarkPackageDirty();
            return true;
        }
    }
    return false;
}

bool UMMAEditorAnimationLibrary::AddMMAChaseLeashComponent(
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
             ExistingNode->ComponentClass == UMMAChaseLeashComponent::StaticClass()))
        {
            return true;
        }
    }

    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UMMAChaseLeashComponent::StaticClass(),
        ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }

    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMMAEditorAnimationLibrary::AddMMAHedgeTrimmerBehaviorComponent(
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
             ExistingNode->ComponentClass == UMMAHedgeTrimmerBehaviorComponent::StaticClass()))
        {
            return true;
        }
    }

    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UMMAHedgeTrimmerBehaviorComponent::StaticClass(),
        ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }

    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMMAEditorAnimationLibrary::AddMMAShieldGuardBehaviorComponent(
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
             ExistingNode->ComponentClass == UMMAShieldGuardBehaviorComponent::StaticClass()))
        {
            return true;
        }
    }
    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UMMAShieldGuardBehaviorComponent::StaticClass(),
        ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }
    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMMAEditorAnimationLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}

bool UMMAEditorAnimationLibrary::AddMMAGreenDruidBehaviorComponent(
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
             ExistingNode->ComponentClass == UMMAGreenDruidBehaviorComponent::StaticClass()))
        {
            return true;
        }
    }
    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UMMAGreenDruidBehaviorComponent::StaticClass(), ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }
    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMMAEditorAnimationLibrary::AddClubAttackAlertTargetGuard(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }

    static const FString GuardComment =
        TEXT("Guard: only evaluate nearest-player logic when an alert target exists");

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph || Graph->GetFName() != UEdGraphSchema_K2::GN_EventGraph)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeComment == GuardComment)
            {
                return true;
            }
        }

        UK2Node_CallFunction* NearestPlayerCall = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call &&
                Call->GetNodeTitle(ENodeTitleType::ListView).ToString() ==
                    TEXT("Get Nearest Player Im Alert To"))
            {
                NearestPlayerCall = Call;
                break;
            }
        }
        if (!NearestPlayerCall)
        {
            continue;
        }

        UEdGraphPin* NearestExec = NearestPlayerCall->GetExecPin();
        UEdGraphPin* NearestSelf = NearestPlayerCall->FindPin(UEdGraphSchema_K2::PN_Self);
        if (!NearestExec || NearestExec->LinkedTo.Num() != 1 ||
            !NearestSelf || NearestSelf->LinkedTo.Num() != 1)
        {
            return false;
        }

        UEdGraphPin* UpstreamExec = NearestExec->LinkedTo[0];
        UEdGraphPin* CharacterReference = NearestSelf->LinkedTo[0];
        UClass* CharacterClass = Cast<UClass>(CharacterReference->PinType.PinSubCategoryObject.Get());
        if (!CharacterClass ||
            !CharacterClass->FindPropertyByName(TEXT("Players_I_Am_Alert_To")))
        {
            return false;
        }

        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
        if (!Schema)
        {
            return false;
        }

        Blueprint->Modify();
        Graph->Modify();

        FGraphNodeCreator<UK2Node_VariableGet> TargetsCreator(*Graph);
        UK2Node_VariableGet* TargetsNode = TargetsCreator.CreateNode();
        TargetsNode->VariableReference.SetExternalMember(
            TEXT("Players_I_Am_Alert_To"), CharacterClass);
        TargetsNode->NodePosX = NearestPlayerCall->NodePosX - 256;
        TargetsNode->NodePosY = NearestPlayerCall->NodePosY + 576;
        TargetsCreator.Finalize();

        FGraphNodeCreator<UK2Node_CallArrayFunction> LengthCreator(*Graph);
        UK2Node_CallArrayFunction* LengthNode = LengthCreator.CreateNode();
        LengthNode->SetFromFunction(
            UKismetArrayLibrary::StaticClass()->FindFunctionByName(
                GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
        LengthNode->NodePosX = NearestPlayerCall->NodePosX;
        LengthNode->NodePosY = NearestPlayerCall->NodePosY + 576;
        LengthCreator.Finalize();

        FGraphNodeCreator<UK2Node_CallFunction> HasTargetsCreator(*Graph);
        UK2Node_CallFunction* HasTargetsNode = HasTargetsCreator.CreateNode();
        HasTargetsNode->SetFromFunction(
            UKismetMathLibrary::StaticClass()->FindFunctionByName(
                GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_IntInt)));
        HasTargetsNode->NodePosX = NearestPlayerCall->NodePosX + 240;
        HasTargetsNode->NodePosY = NearestPlayerCall->NodePosY + 576;
        HasTargetsCreator.Finalize();

        FGraphNodeCreator<UK2Node_IfThenElse> GuardCreator(*Graph);
        UK2Node_IfThenElse* GuardNode = GuardCreator.CreateNode();
        GuardNode->NodePosX = NearestPlayerCall->NodePosX - 224;
        GuardNode->NodePosY = NearestPlayerCall->NodePosY - 192;
        GuardNode->NodeComment = GuardComment;
        GuardNode->bCommentBubbleVisible = true;
        GuardCreator.Finalize();

        UEdGraphPin* TargetsSelf = TargetsNode->FindPin(UEdGraphSchema_K2::PN_Self);
        UEdGraphPin* TargetsValue = TargetsNode->GetValuePin();
        UEdGraphPin* LengthArray = LengthNode->GetTargetArrayPin();
        UEdGraphPin* LengthResult = LengthNode->GetReturnValuePin();
        UEdGraphPin* ComparisonA = HasTargetsNode->FindPin(TEXT("A"));
        UEdGraphPin* ComparisonB = HasTargetsNode->FindPin(TEXT("B"));
        UEdGraphPin* ComparisonResult = HasTargetsNode->GetReturnValuePin();
        if (!TargetsSelf || !TargetsValue || !LengthArray || !LengthResult ||
            !ComparisonA || !ComparisonB || !ComparisonResult)
        {
            return false;
        }

        ComparisonB->DefaultValue = TEXT("0");
        UpstreamExec->BreakLinkTo(NearestExec);

        const bool bConnected =
            Schema->TryCreateConnection(CharacterReference, TargetsSelf) &&
            Schema->TryCreateConnection(TargetsValue, LengthArray) &&
            Schema->TryCreateConnection(LengthResult, ComparisonA) &&
            Schema->TryCreateConnection(ComparisonResult, GuardNode->GetConditionPin()) &&
            Schema->TryCreateConnection(UpstreamExec, GuardNode->GetExecPin()) &&
            Schema->TryCreateConnection(GuardNode->GetThenPin(), NearestExec);
        LengthNode->PinConnectionListChanged(LengthArray);
        if (!bConnected)
        {
            return false;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        return Blueprint->Status != BS_Error;
    }

    return false;
}

bool UMMAEditorAnimationLibrary::AddNearestAlertPlayerEmptyArrayGuard(
    UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }

    static const FName FunctionGraphName(TEXT("Get Nearest Player Im Alert To"));
    static const FString GuardComment =
        TEXT("Guard: return None without indexing when the alert-target array is empty");

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph || Graph->GetFName() != FunctionGraphName)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeComment == GuardComment)
            {
                return true;
            }
        }

        UK2Node_FunctionEntry* EntryNode = nullptr;
        UK2Node_FunctionResult* ExistingResult = nullptr;
        UK2Node_VariableGet* TargetsNode = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!EntryNode)
            {
                EntryNode = Cast<UK2Node_FunctionEntry>(Node);
            }
            if (!ExistingResult)
            {
                ExistingResult = Cast<UK2Node_FunctionResult>(Node);
            }
            UK2Node_VariableGet* VariableGet = Cast<UK2Node_VariableGet>(Node);
            if (VariableGet &&
                VariableGet->GetNodeTitle(ENodeTitleType::ListView).ToString() ==
                    TEXT("Get Players_I_Am_Alert_To"))
            {
                TargetsNode = VariableGet;
            }
        }

        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
        UEdGraphPin* EntryThen = EntryNode && Schema
            ? Schema->FindExecutionPin(*EntryNode, EGPD_Output)
            : nullptr;
        UEdGraphPin* TargetsValue = TargetsNode ? TargetsNode->GetValuePin() : nullptr;
        if (!Schema || !EntryNode || !ExistingResult || !TargetsNode ||
            !EntryThen || EntryThen->LinkedTo.Num() != 1 || !TargetsValue)
        {
            return false;
        }
        UEdGraphPin* OriginalFirstExec = EntryThen->LinkedTo[0];

        Blueprint->Modify();
        Graph->Modify();

        FGraphNodeCreator<UK2Node_CallArrayFunction> LengthCreator(*Graph);
        UK2Node_CallArrayFunction* LengthNode = LengthCreator.CreateNode();
        LengthNode->SetFromFunction(
            UKismetArrayLibrary::StaticClass()->FindFunctionByName(
                GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
        LengthNode->NodePosX = EntryNode->NodePosX + 224;
        LengthNode->NodePosY = EntryNode->NodePosY + 208;
        LengthCreator.Finalize();

        FGraphNodeCreator<UK2Node_CallFunction> HasTargetsCreator(*Graph);
        UK2Node_CallFunction* HasTargetsNode = HasTargetsCreator.CreateNode();
        HasTargetsNode->SetFromFunction(
            UKismetMathLibrary::StaticClass()->FindFunctionByName(
                GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_IntInt)));
        HasTargetsNode->NodePosX = EntryNode->NodePosX + 448;
        HasTargetsNode->NodePosY = EntryNode->NodePosY + 208;
        HasTargetsCreator.Finalize();

        FGraphNodeCreator<UK2Node_IfThenElse> GuardCreator(*Graph);
        UK2Node_IfThenElse* GuardNode = GuardCreator.CreateNode();
        GuardNode->NodePosX = EntryNode->NodePosX + 224;
        GuardNode->NodePosY = EntryNode->NodePosY - 16;
        GuardNode->NodeComment = GuardComment;
        GuardNode->bCommentBubbleVisible = true;
        GuardCreator.Finalize();

        FGraphNodeCreator<UK2Node_FunctionResult> EmptyResultCreator(*Graph);
        UK2Node_FunctionResult* EmptyResult = EmptyResultCreator.CreateNode();
        EmptyResult->FunctionReference = EntryNode->FunctionReference;
        EmptyResult->NodePosX = EntryNode->NodePosX + 704;
        EmptyResult->NodePosY = EntryNode->NodePosY + 352;
        EmptyResultCreator.Finalize();

        UEdGraphPin* LengthArray = LengthNode->GetTargetArrayPin();
        UEdGraphPin* LengthResult = LengthNode->GetReturnValuePin();
        UEdGraphPin* ComparisonA = HasTargetsNode->FindPin(TEXT("A"));
        UEdGraphPin* ComparisonB = HasTargetsNode->FindPin(TEXT("B"));
        UEdGraphPin* ComparisonResult = HasTargetsNode->GetReturnValuePin();
        UEdGraphPin* EmptyResultExec =
            Schema->FindExecutionPin(*EmptyResult, EGPD_Input);
        if (!LengthArray || !LengthResult || !ComparisonA || !ComparisonB ||
            !ComparisonResult || !EmptyResultExec)
        {
            return false;
        }

        ComparisonB->DefaultValue = TEXT("0");
        EntryThen->BreakLinkTo(OriginalFirstExec);
        const bool bConnected =
            Schema->TryCreateConnection(TargetsValue, LengthArray) &&
            Schema->TryCreateConnection(LengthResult, ComparisonA) &&
            Schema->TryCreateConnection(ComparisonResult, GuardNode->GetConditionPin()) &&
            Schema->TryCreateConnection(EntryThen, GuardNode->GetExecPin()) &&
            Schema->TryCreateConnection(GuardNode->GetThenPin(), OriginalFirstExec) &&
            Schema->TryCreateConnection(GuardNode->GetElsePin(), EmptyResultExec);
        LengthNode->PinConnectionListChanged(LengthArray);
        if (!bConnected)
        {
            return false;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        return Blueprint->Status != BS_Error;
    }

    return false;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAHedgeTrimmerBehavior(
    UBlueprint* Blueprint,
    UAnimSequence* IdleAnimation,
    UAnimSequence* NoticeAnimation,
    UAnimSequence* ChaseAnimation,
    UAnimSequence* AttackAnimation,
    UAnimSequence* ReturnHomeAnimation,
    UAnimSequence* DeathAnimation,
    TSubclassOf<AActor> DefaultDropClass)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    bool bFoundBehavior = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName() == TEXT("Weapon Hitbox"))
        {
            if (UPrimitiveComponent* WeaponHitbox = Cast<UPrimitiveComponent>(Node->ComponentTemplate))
            {
                Node->Modify();
                WeaponHitbox->Modify();
                WeaponHitbox->SetGenerateOverlapEvents(false);
                WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        UMMAHedgeTrimmerBehaviorComponent* Behavior = Node
            ? Cast<UMMAHedgeTrimmerBehaviorComponent>(Node->ComponentTemplate)
            : nullptr;
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        Behavior->IdleAnimation = IdleAnimation;
        Behavior->NoticeAnimation = NoticeAnimation;
        Behavior->ChaseAnimation = ChaseAnimation;
        Behavior->AttackAnimation = AttackAnimation;
        Behavior->ReturnHomeAnimation = ReturnHomeAnimation;
        Behavior->DeathAnimation = DeathAnimation;
        Behavior->DefaultDropClass = DefaultDropClass;
        Behavior->AttackRange = 135.0f;
        Behavior->AttackHitRange = 170.0f;
        Behavior->AttackContactSeconds = 0.105f;
        Behavior->RecoilHorizontalSpeed = 260.0f;
        Behavior->RecoilVerticalSpeed = 140.0f;
        Behavior->DeathPoofPaddingSeconds = 0.25f;
        Behavior->bEnableDebugMessages = true;
        bFoundBehavior = true;
    }
    if (bFoundBehavior)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bFoundBehavior;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAEnemyDeathTerminalAnimation(
    UBlueprint* Blueprint,
    UAnimSequence* DeathTerminalAnimation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    bool bFoundBehavior = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UMMAHedgeTrimmerBehaviorComponent* Behavior = Node
            ? Cast<UMMAHedgeTrimmerBehaviorComponent>(Node->ComponentTemplate)
            : nullptr;
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        Behavior->DeathTerminalAnimation = DeathTerminalAnimation;
        bFoundBehavior = true;
    }
    if (bFoundBehavior)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bFoundBehavior;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAEnemyStateMachineSettings(
    UBlueprint* Blueprint,
    const FString& SettingsJson)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    TSharedPtr<FJsonObject> Settings;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SettingsJson);
    if (!FJsonSerializer::Deserialize(Reader, Settings) || !Settings.IsValid())
    {
        return false;
    }

    bool bConfigured = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UMMAHedgeTrimmerBehaviorComponent* Behavior = Node
            ? Cast<UMMAHedgeTrimmerBehaviorComponent>(Node->ComponentTemplate)
            : nullptr;
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        auto SetNumber = [&Settings](const TCHAR* Key, float& Target)
        {
            double Value = 0.0;
            if (Settings->TryGetNumberField(Key, Value))
            {
                Target = static_cast<float>(Value);
            }
        };
        auto SetBool = [&Settings](const TCHAR* Key, bool& Target)
        {
            bool Value = false;
            if (Settings->TryGetBoolField(Key, Value))
            {
                Target = Value;
            }
        };
        SetNumber(TEXT("detection_radius"), Behavior->DetectionRadius);
        SetNumber(TEXT("lose_interest_radius"), Behavior->LoseInterestRadius);
        SetBool(TEXT("require_line_of_sight"), Behavior->bRequireLineOfSight);
        SetBool(TEXT("require_target_reentry_after_hit"), Behavior->bRequireTargetReentryAfterHit);
        SetNumber(TEXT("target_rearm_radius"), Behavior->TargetRearmRadius);
        SetNumber(TEXT("attack_target_clearance"), Behavior->AttackTargetClearance);
        SetNumber(TEXT("chase_speed"), Behavior->ChaseSpeed);
        SetNumber(TEXT("return_home_speed"), Behavior->ReturnHomeSpeed);
        SetNumber(TEXT("maximum_distance_from_home"), Behavior->MaximumDistanceFromHome);
        SetNumber(TEXT("home_acceptance_radius"), Behavior->HomeAcceptanceRadius);
        SetNumber(TEXT("rotation_speed_degrees"), Behavior->RotationSpeedDegrees);
        SetNumber(TEXT("attack_range"), Behavior->AttackRange);
        SetNumber(TEXT("attack_hit_range"), Behavior->AttackHitRange);
        SetNumber(TEXT("attack_half_angle_degrees"), Behavior->AttackHalfAngleDegrees);
        SetNumber(TEXT("attack_contact_seconds"), Behavior->AttackContactSeconds);
        SetNumber(TEXT("attack_cooldown_seconds"), Behavior->AttackCooldownSeconds);
        SetNumber(TEXT("hit_points"), Behavior->InitialHitPoints);
        SetNumber(TEXT("recoil_horizontal_speed"), Behavior->RecoilHorizontalSpeed);
        SetNumber(TEXT("recoil_vertical_speed"), Behavior->RecoilVerticalSpeed);
        SetNumber(TEXT("death_poof_padding_seconds"), Behavior->DeathPoofPaddingSeconds);
        SetNumber(TEXT("death_playback_rate"), Behavior->DeathPlaybackRate);
        SetNumber(TEXT("death_animation_end_fraction"), Behavior->DeathAnimationEndFraction);
        SetNumber(TEXT("death_terminal_duration_seconds"), Behavior->DeathTerminalDurationSeconds);
        SetNumber(TEXT("death_terminal_backward_distance"), Behavior->DeathTerminalBackwardDistance);
        SetNumber(TEXT("death_terminal_upward_distance"), Behavior->DeathTerminalUpwardDistance);
        SetNumber(TEXT("death_terminal_end_scale"), Behavior->DeathTerminalEndScale);
        SetBool(TEXT("debug_messages"), Behavior->bEnableDebugMessages);
        FString DamageType;
        if (Settings->TryGetStringField(TEXT("damage_type"), DamageType) &&
            DamageType == TEXT("NORMAL_DAMAGE"))
        {
            Behavior->bOverrideOutgoingDamageType = true;
            Behavior->OutgoingDamageType = 1;
        }
        bConfigured = true;
    }
    if (bConfigured)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bConfigured;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAShieldGuardBehavior(
    UBlueprint* Blueprint,
    UAnimSequence* IdleAnimation,
    UAnimSequence* PatrolAnimation,
    UAnimSequence* EnGardeAnimation,
    UAnimSequence* AttackAnimation,
    UAnimSequence* DeathAnimation,
    TSubclassOf<AActor> DefaultDropClass)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    bool bFoundBehavior = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (!Node)
        {
            continue;
        }
        FString VariableName = Node->GetVariableName().ToString().ToLower();
        VariableName.ReplaceInline(TEXT("_"), TEXT(""));
        VariableName.ReplaceInline(TEXT(" "), TEXT(""));
        if (VariableName.Contains(TEXT("weaponhitbox")))
        {
            if (UPrimitiveComponent* Hitbox = Cast<UPrimitiveComponent>(Node->ComponentTemplate))
            {
                Node->Modify();
                Hitbox->Modify();
                Hitbox->SetGenerateOverlapEvents(false);
                Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        else if (VariableName.Contains(TEXT("shieldhitbox")))
        {
            if (UPrimitiveComponent* Hitbox = Cast<UPrimitiveComponent>(Node->ComponentTemplate))
            {
                Node->Modify();
                Hitbox->Modify();
                Hitbox->SetCollisionObjectType(ECC_WorldStatic);
                Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
                Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                Hitbox->SetGenerateOverlapEvents(true);
                Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            }
        }
        UMMAShieldGuardBehaviorComponent* Behavior =
            Cast<UMMAShieldGuardBehaviorComponent>(Node->ComponentTemplate);
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        Behavior->IdleAnimation = IdleAnimation;
        Behavior->PatrolAnimation = PatrolAnimation;
        Behavior->EnGardeAnimation = EnGardeAnimation;
        Behavior->AttackAnimation = AttackAnimation;
        Behavior->DeathAnimation = DeathAnimation;
        Behavior->DefaultDropClass = DefaultDropClass;
        bFoundBehavior = true;
    }
    if (bFoundBehavior)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bFoundBehavior;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAShieldGuardSettings(
    UBlueprint* Blueprint,
    const FString& SettingsJson)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    TSharedPtr<FJsonObject> Settings;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SettingsJson);
    if (!FJsonSerializer::Deserialize(Reader, Settings) || !Settings.IsValid())
    {
        return false;
    }
    bool bConfigured = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UMMAShieldGuardBehaviorComponent* Behavior = Node
            ? Cast<UMMAShieldGuardBehaviorComponent>(Node->ComponentTemplate)
            : nullptr;
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        auto SetNumber = [&Settings](const TCHAR* Key, float& Target)
        {
            double Value = 0.0;
            if (Settings->TryGetNumberField(Key, Value))
            {
                Target = static_cast<float>(Value);
            }
        };
        auto SetBool = [&Settings](const TCHAR* Key, bool& Target)
        {
            bool Value = false;
            if (Settings->TryGetBoolField(Key, Value))
            {
                Target = Value;
            }
        };
        SetNumber(TEXT("idle_wait_minimum"), Behavior->IdleWaitMinimum);
        SetNumber(TEXT("idle_wait_maximum"), Behavior->IdleWaitMaximum);
        SetNumber(TEXT("patrol_radius"), Behavior->PatrolRadius);
        SetNumber(TEXT("patrol_speed"), Behavior->PatrolSpeed);
        SetNumber(TEXT("patrol_acceptance_radius"), Behavior->PatrolAcceptanceRadius);
        SetNumber(TEXT("patrol_pause_minimum"), Behavior->PatrolPauseMinimum);
        SetNumber(TEXT("patrol_pause_maximum"), Behavior->PatrolPauseMaximum);
        SetNumber(TEXT("patrol_target_timeout"), Behavior->PatrolTargetTimeout);
        SetNumber(TEXT("guard_radius"), Behavior->GuardRadius);
        SetNumber(TEXT("lose_interest_radius"), Behavior->LoseInterestRadius);
        SetBool(TEXT("require_line_of_sight"), Behavior->bRequireLineOfSight);
        SetNumber(TEXT("rotation_speed_degrees"), Behavior->RotationSpeedDegrees);
        SetNumber(TEXT("attack_range"), Behavior->AttackRange);
        SetNumber(TEXT("attack_hit_range"), Behavior->AttackHitRange);
        SetNumber(TEXT("attack_half_angle_degrees"), Behavior->AttackHalfAngleDegrees);
        SetNumber(TEXT("attack_contact_fraction"), Behavior->AttackContactFraction);
        SetNumber(TEXT("attack_cooldown_seconds"), Behavior->AttackCooldownSeconds);
        SetNumber(TEXT("hit_points"), Behavior->InitialHitPoints);
        SetNumber(TEXT("recoil_horizontal_speed"), Behavior->RecoilHorizontalSpeed);
        SetNumber(TEXT("recoil_vertical_speed"), Behavior->RecoilVerticalSpeed);
        SetNumber(TEXT("charge_knockback_horizontal_speed"), Behavior->ChargeKnockbackHorizontalSpeed);
        SetNumber(TEXT("charge_knockback_vertical_speed"), Behavior->ChargeKnockbackVerticalSpeed);
        SetNumber(TEXT("charge_collision_radius_scale"), Behavior->ChargeCollisionRadiusScale);
        SetNumber(TEXT("charge_collision_half_height_scale"), Behavior->ChargeCollisionHalfHeightScale);
        SetNumber(TEXT("death_poof_padding_seconds"), Behavior->DeathPoofPaddingSeconds);
        SetBool(TEXT("immune_to_burn"), Behavior->bImmuneToFlame);
        SetBool(TEXT("immune_to_flame"), Behavior->bImmuneToFlame);
        SetBool(TEXT("debug_messages"), Behavior->bEnableDebugMessages);
        FString DamageType;
        if (Settings->TryGetStringField(TEXT("damage_type"), DamageType) &&
            DamageType == TEXT("NORMAL_DAMAGE"))
        {
            Behavior->OutgoingDamageType = 1;
        }
        bConfigured = true;
    }
    if (bConfigured)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bConfigured;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAHedgeTrimmerMesh(
    UBlueprint* Blueprint,
    USkeletalMesh* SkeletalMesh,
    UAnimSequence* PreviewAnimation,
    FVector RelativeLocation,
    FRotator RelativeRotation,
    FVector RelativeScale)
{
    if (!Blueprint || !Blueprint->GeneratedClass || !SkeletalMesh || !PreviewAnimation)
    {
        return false;
    }
    ACharacter* CDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* Mesh = CDO ? CDO->GetMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    Blueprint->Modify();
    CDO->Modify();
    Mesh->Modify();
    Mesh->SetSkeletalMesh(SkeletalMesh);
    Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Mesh->AnimClass = nullptr;
    Mesh->AnimationData.AnimToPlay = PreviewAnimation;
    Mesh->AnimationData.bSavedLooping = true;
    Mesh->AnimationData.bSavedPlaying = true;
    Mesh->AnimationData.SavedPosition = 0.0f;
    Mesh->AnimationData.SavedPlayRate = 1.0f;
    Mesh->OverrideMaterials.Empty();
    Mesh->SetRelativeLocation(RelativeLocation);
    Mesh->SetRelativeRotation(RelativeRotation);
    Mesh->SetRelativeScale3D(RelativeScale);
    Mesh->MarkRenderStateDirty();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAGreenDruidBehavior(
    UBlueprint* Blueprint,
    UAnimSequence* IdleAnimation,
    UAnimSequence* RaiseAnimation,
    UAnimSequence* LowerAnimation,
    UAnimSequence* DeathAnimation,
    TSubclassOf<AActor> DefaultDropClass)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    bool bConfigured = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UMMAGreenDruidBehaviorComponent* Behavior = Node
            ? Cast<UMMAGreenDruidBehaviorComponent>(Node->ComponentTemplate) : nullptr;
        if (!Behavior)
        {
            continue;
        }
        Node->Modify();
        Behavior->Modify();
        Behavior->IdleAnimation = IdleAnimation;
        Behavior->RaiseAnimation = RaiseAnimation;
        Behavior->LowerAnimation = LowerAnimation ? LowerAnimation : RaiseAnimation;
        Behavior->DeathAnimation = DeathAnimation;
        Behavior->DefaultDropClass = DefaultDropClass;
        Behavior->ActivationRadius = 1000.0f;
        Behavior->DeactivationRadius = 1200.0f;
        Behavior->TransitionDuration = 1.25f;
        Behavior->RaisedHoldDuration = 2.0f;
        Behavior->FlatHoldDuration = 1.5f;
        Behavior->InitialHitPoints = 1.0f;
        Behavior->bPlayLowerAnimationReversed = true;
        bConfigured = true;
    }
    if (bConfigured)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bConfigured;
}

namespace
{
float TranslationRange(const FRawAnimSequenceTrack& Track)
{
    if (Track.PosKeys.Num() < 2)
    {
        return 0.0f;
    }
    FBox Bounds(ForceInit);
    for (const FVector& Position : Track.PosKeys)
    {
        Bounds += Position;
    }
    return Bounds.IsValid ? Bounds.GetExtent().Size() * 2.0f : 0.0f;
}

float RawTrackMotionScore(const FRawAnimSequenceTrack& Track)
{
    float Score = TranslationRange(Track);
    if (Track.RotKeys.Num() > 1)
    {
        const FQuat First = Track.RotKeys[0];
        for (const FQuat& Rotation : Track.RotKeys)
        {
            Score += FMath::RadiansToDegrees(First.AngularDistance(Rotation));
        }
    }
    return Score;
}
}

bool UMMAEditorAnimationLibrary::MergeAnimationTracks(
    UAnimSequence* Destination,
    UAnimSequence* AdditionalTracks)
{
    if (!Destination || !AdditionalTracks ||
        Destination == AdditionalTracks ||
        Destination->GetSkeleton() != AdditionalTracks->GetSkeleton())
    {
        return false;
    }
    Destination->Modify();
    bool bChanged = false;
    for (int32 SourceIndex = 0;
        SourceIndex < AdditionalTracks->GetAnimationTrackNames().Num() &&
        SourceIndex < AdditionalTracks->GetRawAnimationData().Num();
        ++SourceIndex)
    {
        const FName TrackName = AdditionalTracks->GetAnimationTrackNames()[SourceIndex];
        const FRawAnimSequenceTrack& SourceTrack = AdditionalTracks->GetRawAnimationTrack(SourceIndex);
        const int32 DestinationIndex = Destination->GetAnimationTrackNames().Find(TrackName);
        if (DestinationIndex == INDEX_NONE)
        {
            FRawAnimSequenceTrack Copy = SourceTrack;
            Destination->AddNewRawTrack(TrackName, &Copy);
            bChanged = true;
        }
        else if (Destination->GetRawAnimationData().IsValidIndex(DestinationIndex) &&
            RawTrackMotionScore(SourceTrack) >
                RawTrackMotionScore(Destination->GetRawAnimationTrack(DestinationIndex)) + KINDA_SMALL_NUMBER)
        {
            Destination->GetRawAnimationTrack(DestinationIndex) = SourceTrack;
            bChanged = true;
        }
    }
    if (bChanged)
    {
        Destination->MarkRawDataAsModified();
        Destination->PostProcessSequence();
        Destination->MarkPackageDirty();
        Destination->PostEditChange();
    }
    return bChanged;
}

FName UMMAEditorAnimationLibrary::FindLargestTranslationTrackBone(UAnimSequence* Animation)
{
    if (!Animation)
    {
        return NAME_None;
    }
    FName BestName = NAME_None;
    float BestRange = 0.0f;
    for (int32 Index = 0;
        Index < Animation->GetAnimationTrackNames().Num() && Index < Animation->GetRawAnimationData().Num();
        ++Index)
    {
        const float Range = TranslationRange(Animation->GetRawAnimationTrack(Index));
        if (Range > BestRange)
        {
            BestRange = Range;
            BestName = Animation->GetAnimationTrackNames()[Index];
        }
    }
    return BestName;
}

bool UMMAEditorAnimationLibrary::ConfigureMMAGreenDruidPlatform(
    UBlueprint* Blueprint,
    USkeletalMesh* SkeletalMesh,
    UAnimSequence* LiftAnimation,
    FName LiftBoneName)
{
    if (!Blueprint || !Blueprint->GeneratedClass || !SkeletalMesh || !LiftAnimation)
    {
        return false;
    }
    AMMAGreenDruidPlatform* CDO = Cast<AMMAGreenDruidPlatform>(
        Blueprint->GeneratedClass->GetDefaultObject());
    if (!CDO || !CDO->PlatformVisual || !CDO->RideSurface || !CDO->ColumnBlocker)
    {
        return false;
    }
    Blueprint->Modify();
    CDO->Modify();
    CDO->PlatformVisual->Modify();
    CDO->PlatformVisual->SetSkeletalMesh(SkeletalMesh);
    CDO->PlatformVisual->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    CDO->PlatformVisual->AnimationData.AnimToPlay = LiftAnimation;
    CDO->PlatformVisual->AnimationData.bSavedLooping = false;
    CDO->PlatformVisual->AnimationData.bSavedPlaying = false;
    CDO->LiftAnimation = LiftAnimation;
    CDO->LiftBoneName = LiftBoneName;

    const FBoxSphereBounds Bounds = SkeletalMesh->GetBounds();
    CDO->FlatSurfaceRelativeLocation = FVector(
        Bounds.Origin.X,
        Bounds.Origin.Y,
        Bounds.Origin.Z + Bounds.BoxExtent.Z);
    CDO->RideSurfaceBoxExtent = FVector(
        FMath::Max(10.0f, Bounds.BoxExtent.X),
        FMath::Max(10.0f, Bounds.BoxExtent.Y),
        12.0f);

    float LiftRange = 0.0f;
    const int32 TrackIndex = LiftAnimation->GetAnimationTrackNames().Find(LiftBoneName);
    if (LiftAnimation->GetRawAnimationData().IsValidIndex(TrackIndex))
    {
        LiftRange = TranslationRange(LiftAnimation->GetRawAnimationTrack(TrackIndex));
    }
    CDO->FallbackLiftHeight = FMath::Max(1.0f, LiftRange);
    CDO->RideSurface->SetBoxExtent(CDO->RideSurfaceBoxExtent);
    CDO->RideSurface->SetRelativeLocation(CDO->FlatSurfaceRelativeLocation);
    CDO->PayloadRoot->SetRelativeLocation(CDO->FlatSurfaceRelativeLocation);
    CDO->PlatformVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

FString UMMAEditorAnimationLibrary::DescribeMMAGreenDruidBlueprint(UBlueprint* Blueprint)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    if (!Blueprint)
    {
        Root->SetStringField(TEXT("error"), TEXT("null blueprint"));
    }
    else
    {
        Root->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
        Root->SetStringField(TEXT("status"), UEnum::GetValueAsString(Blueprint->Status));
        Root->SetStringField(TEXT("parent_class"),
            Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
        if (Blueprint->GeneratedClass)
        {
            if (AMMAGreenDruidPlatform* Platform = Cast<AMMAGreenDruidPlatform>(
                Blueprint->GeneratedClass->GetDefaultObject()))
            {
                Root->SetStringField(TEXT("kind"), TEXT("platform"));
                Root->SetStringField(TEXT("mesh"), Platform->PlatformVisual && Platform->PlatformVisual->SkeletalMesh
                    ? Platform->PlatformVisual->SkeletalMesh->GetPathName() : FString());
                Root->SetStringField(TEXT("lift_animation"),
                    Platform->LiftAnimation ? Platform->LiftAnimation->GetPathName() : FString());
                Root->SetStringField(TEXT("lift_bone"), Platform->LiftBoneName.ToString());
                Root->SetNumberField(TEXT("fallback_lift_height"), Platform->FallbackLiftHeight);
            }
        }
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                UMMAGreenDruidBehaviorComponent* Behavior = Node
                    ? Cast<UMMAGreenDruidBehaviorComponent>(Node->ComponentTemplate) : nullptr;
                if (!Behavior)
                {
                    continue;
                }
                Root->SetStringField(TEXT("kind"), TEXT("enemy"));
                Root->SetNumberField(TEXT("activation_radius"), Behavior->ActivationRadius);
                Root->SetNumberField(TEXT("deactivation_radius"), Behavior->DeactivationRadius);
                Root->SetNumberField(TEXT("transition_duration"), Behavior->TransitionDuration);
                Root->SetNumberField(TEXT("raised_hold_duration"), Behavior->RaisedHoldDuration);
                Root->SetNumberField(TEXT("flat_hold_duration"), Behavior->FlatHoldDuration);
                Root->SetStringField(TEXT("idle"), Behavior->IdleAnimation ? Behavior->IdleAnimation->GetPathName() : FString());
                Root->SetStringField(TEXT("raise"), Behavior->RaiseAnimation ? Behavior->RaiseAnimation->GetPathName() : FString());
                Root->SetStringField(TEXT("lower"), Behavior->LowerAnimation ? Behavior->LowerAnimation->GetPathName() : FString());
                Root->SetStringField(TEXT("death"), Behavior->DeathAnimation ? Behavior->DeathAnimation->GetPathName() : FString());
            }
        }
    }
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

FString UMMAEditorAnimationLibrary::DescribeMMAHedgeTrimmerBlueprint(UBlueprint* Blueprint)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    if (!Blueprint)
    {
        Root->SetStringField(TEXT("error"), TEXT("null blueprint"));
    }
    else
    {
        Root->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
        Root->SetStringField(
            TEXT("parent_class"),
            Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT(""));
        Root->SetStringField(TEXT("status"), UEnum::GetValueAsString(Blueprint->Status));

        TArray<TSharedPtr<FJsonValue>> Nodes;
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node)
                {
                    continue;
                }
                TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
                NodeObject->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
                NodeObject->SetStringField(
                    TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT(""));
                if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Node->ComponentTemplate))
                {
                    NodeObject->SetStringField(
                        TEXT("collision_profile"), Primitive->GetCollisionProfileName().ToString());
                    NodeObject->SetStringField(
                        TEXT("collision_enabled"), UEnum::GetValueAsString(Primitive->GetCollisionEnabled()));
                    NodeObject->SetStringField(
                        TEXT("collision_object_type"), UEnum::GetValueAsString(Primitive->GetCollisionObjectType()));
                    NodeObject->SetStringField(
                        TEXT("pawn_response"),
                        UEnum::GetValueAsString(Primitive->GetCollisionResponseToChannel(ECC_Pawn)));
                    NodeObject->SetBoolField(
                        TEXT("generate_overlap_events"), Primitive->GetGenerateOverlapEvents());
                }
                if (UMMAHedgeTrimmerBehaviorComponent* Behavior =
                        Cast<UMMAHedgeTrimmerBehaviorComponent>(Node->ComponentTemplate))
                {
                    TSharedRef<FJsonObject> Contract = MakeShared<FJsonObject>();
                    auto AssetPath = [](const UObject* Asset)
                    {
                        return Asset ? Asset->GetPathName() : FString();
                    };
                    Contract->SetStringField(TEXT("idle"), AssetPath(Behavior->IdleAnimation));
                    Contract->SetStringField(TEXT("notice"), AssetPath(Behavior->NoticeAnimation));
                    Contract->SetStringField(TEXT("chase"), AssetPath(Behavior->ChaseAnimation));
                    Contract->SetStringField(TEXT("attack"), AssetPath(Behavior->AttackAnimation));
                    Contract->SetStringField(TEXT("return_home"), AssetPath(Behavior->ReturnHomeAnimation));
                    Contract->SetStringField(TEXT("death"), AssetPath(Behavior->DeathAnimation));
                    Contract->SetStringField(
                        TEXT("death_terminal"), AssetPath(Behavior->DeathTerminalAnimation));
                    Contract->SetStringField(
                        TEXT("death_poof_particle"), AssetPath(Behavior->DeathPoofParticle));
                    Contract->SetStringField(
                        TEXT("death_poof_sound"), AssetPath(Behavior->DeathPoofSound));
                    Contract->SetStringField(TEXT("default_drop"), AssetPath(Behavior->DefaultDropClass.Get()));
                    Contract->SetNumberField(TEXT("detection_radius"), Behavior->DetectionRadius);
                    Contract->SetNumberField(TEXT("lose_interest_radius"), Behavior->LoseInterestRadius);
                    Contract->SetBoolField(
                        TEXT("require_target_reentry_after_hit"),
                        Behavior->bRequireTargetReentryAfterHit);
                    Contract->SetNumberField(TEXT("target_rearm_radius"), Behavior->TargetRearmRadius);
                    Contract->SetNumberField(TEXT("attack_target_clearance"), Behavior->AttackTargetClearance);
                    Contract->SetNumberField(TEXT("chase_speed"), Behavior->ChaseSpeed);
                    Contract->SetNumberField(TEXT("return_home_speed"), Behavior->ReturnHomeSpeed);
                    Contract->SetNumberField(TEXT("maximum_distance_from_home"), Behavior->MaximumDistanceFromHome);
                    Contract->SetNumberField(TEXT("attack_range"), Behavior->AttackRange);
                    Contract->SetNumberField(TEXT("attack_hit_range"), Behavior->AttackHitRange);
                    Contract->SetNumberField(TEXT("attack_contact_seconds"), Behavior->AttackContactSeconds);
                    Contract->SetNumberField(TEXT("attack_cooldown_seconds"), Behavior->AttackCooldownSeconds);
                    Contract->SetNumberField(TEXT("hit_points"), Behavior->InitialHitPoints);
                    Contract->SetBoolField(TEXT("override_outgoing_damage_type"), Behavior->bOverrideOutgoingDamageType);
                    Contract->SetNumberField(TEXT("outgoing_damage_type"), Behavior->OutgoingDamageType);
                    Contract->SetNumberField(TEXT("recoil_horizontal_speed"), Behavior->RecoilHorizontalSpeed);
                    Contract->SetNumberField(TEXT("recoil_vertical_speed"), Behavior->RecoilVerticalSpeed);
                    Contract->SetNumberField(TEXT("death_poof_padding_seconds"), Behavior->DeathPoofPaddingSeconds);
                    Contract->SetNumberField(TEXT("death_playback_rate"), Behavior->DeathPlaybackRate);
                    Contract->SetNumberField(
                        TEXT("death_animation_end_fraction"), Behavior->DeathAnimationEndFraction);
                    Contract->SetNumberField(
                        TEXT("death_terminal_duration_seconds"), Behavior->DeathTerminalDurationSeconds);
                    Contract->SetNumberField(
                        TEXT("death_terminal_backward_distance"), Behavior->DeathTerminalBackwardDistance);
                    Contract->SetNumberField(
                        TEXT("death_terminal_upward_distance"), Behavior->DeathTerminalUpwardDistance);
                    Contract->SetNumberField(
                        TEXT("death_terminal_end_scale"), Behavior->DeathTerminalEndScale);
                    Contract->SetBoolField(TEXT("debug_messages"), Behavior->bEnableDebugMessages);
                    NodeObject->SetObjectField(TEXT("behavior_contract"), Contract);
                }
                else if (UMMAShieldGuardBehaviorComponent* ShieldBehavior =
                             Cast<UMMAShieldGuardBehaviorComponent>(Node->ComponentTemplate))
                {
                    TSharedRef<FJsonObject> Contract = MakeShared<FJsonObject>();
                    auto AssetPath = [](const UObject* Asset)
                    {
                        return Asset ? Asset->GetPathName() : FString();
                    };
                    Contract->SetStringField(TEXT("idle"), AssetPath(ShieldBehavior->IdleAnimation));
                    Contract->SetStringField(TEXT("patrol"), AssetPath(ShieldBehavior->PatrolAnimation));
                    Contract->SetStringField(TEXT("en_garde"), AssetPath(ShieldBehavior->EnGardeAnimation));
                    Contract->SetStringField(TEXT("attack"), AssetPath(ShieldBehavior->AttackAnimation));
                    Contract->SetStringField(TEXT("death"), AssetPath(ShieldBehavior->DeathAnimation));
                    Contract->SetStringField(TEXT("default_drop"), AssetPath(ShieldBehavior->DefaultDropClass.Get()));
                    Contract->SetNumberField(TEXT("idle_wait_minimum"), ShieldBehavior->IdleWaitMinimum);
                    Contract->SetNumberField(TEXT("idle_wait_maximum"), ShieldBehavior->IdleWaitMaximum);
                    Contract->SetNumberField(TEXT("patrol_radius"), ShieldBehavior->PatrolRadius);
                    Contract->SetNumberField(TEXT("patrol_speed"), ShieldBehavior->PatrolSpeed);
                    Contract->SetNumberField(TEXT("guard_radius"), ShieldBehavior->GuardRadius);
                    Contract->SetNumberField(TEXT("lose_interest_radius"), ShieldBehavior->LoseInterestRadius);
                    Contract->SetNumberField(TEXT("attack_range"), ShieldBehavior->AttackRange);
                    Contract->SetNumberField(TEXT("attack_hit_range"), ShieldBehavior->AttackHitRange);
                    Contract->SetNumberField(TEXT("attack_contact_fraction"), ShieldBehavior->AttackContactFraction);
                    Contract->SetNumberField(TEXT("attack_cooldown_seconds"), ShieldBehavior->AttackCooldownSeconds);
                    Contract->SetNumberField(TEXT("outgoing_damage_type"), ShieldBehavior->OutgoingDamageType);
                    Contract->SetBoolField(TEXT("immune_to_burn"), ShieldBehavior->bImmuneToFlame);
                    Contract->SetBoolField(TEXT("immune_to_flame"), ShieldBehavior->bImmuneToFlame);
                    Contract->SetNumberField(TEXT("charge_knockback_horizontal_speed"), ShieldBehavior->ChargeKnockbackHorizontalSpeed);
                    Contract->SetNumberField(TEXT("charge_knockback_vertical_speed"), ShieldBehavior->ChargeKnockbackVerticalSpeed);
                    Contract->SetNumberField(TEXT("charge_collision_radius_scale"), ShieldBehavior->ChargeCollisionRadiusScale);
                    Contract->SetNumberField(TEXT("charge_collision_half_height_scale"), ShieldBehavior->ChargeCollisionHalfHeightScale);
                    Contract->SetNumberField(TEXT("recoil_horizontal_speed"), ShieldBehavior->RecoilHorizontalSpeed);
                    Contract->SetNumberField(TEXT("recoil_vertical_speed"), ShieldBehavior->RecoilVerticalSpeed);
                    Contract->SetNumberField(TEXT("death_poof_padding_seconds"), ShieldBehavior->DeathPoofPaddingSeconds);
                    Contract->SetBoolField(TEXT("debug_messages"), ShieldBehavior->bEnableDebugMessages);
                    NodeObject->SetObjectField(TEXT("behavior_contract"), Contract);
                }
                Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
            }
        }
        Root->SetArrayField(TEXT("scs_nodes"), Nodes);

        UClass* GeneratedClass = Blueprint->GeneratedClass;
        Root->SetBoolField(
            TEXT("has_damage_check_function"),
            GeneratedClass && (GeneratedClass->FindFunctionByName(TEXT("Damage Check")) ||
                               GeneratedClass->FindFunctionByName(TEXT("Damage_Check"))));
        Root->SetBoolField(
            TEXT("has_damageable_property"),
            GeneratedClass && FindFProperty<FProperty>(GeneratedClass, TEXT("Damageable")) != nullptr);
        Root->SetBoolField(
            TEXT("has_drops_items_property"),
            GeneratedClass && FindFProperty<FProperty>(GeneratedClass, TEXT("Drops_Items")) != nullptr);
        Root->SetBoolField(
            TEXT("has_ai_state_property"),
            GeneratedClass && FindFProperty<FProperty>(GeneratedClass, TEXT("AICharacter_State")) != nullptr);
        Root->SetBoolField(
            TEXT("has_walking_ai_character_property"),
            GeneratedClass && FindFProperty<FProperty>(GeneratedClass, TEXT("Walking_AI_Character")) != nullptr);

        if (GeneratedClass)
        {
            if (ACharacter* CDO = Cast<ACharacter>(GeneratedClass->GetDefaultObject()))
            {
                if (USkeletalMeshComponent* Mesh = CDO->GetMesh())
                {
                    TSharedRef<FJsonObject> MeshObject = MakeShared<FJsonObject>();
                    MeshObject->SetStringField(
                        TEXT("asset"), Mesh->SkeletalMesh ? Mesh->SkeletalMesh->GetPathName() : TEXT(""));
                    MeshObject->SetStringField(TEXT("location"), Mesh->GetRelativeLocation().ToString());
                    MeshObject->SetStringField(TEXT("rotation"), Mesh->GetRelativeRotation().ToString());
                    MeshObject->SetStringField(TEXT("scale"), Mesh->GetRelativeScale3D().ToString());
                    MeshObject->SetStringField(
                        TEXT("preview_animation"),
                        Mesh->AnimationData.AnimToPlay
                            ? Mesh->AnimationData.AnimToPlay->GetPathName()
                            : TEXT(""));
                    MeshObject->SetBoolField(TEXT("preview_looping"), Mesh->AnimationData.bSavedLooping);
                    MeshObject->SetBoolField(TEXT("preview_playing"), Mesh->AnimationData.bSavedPlaying);
                    MeshObject->SetNumberField(TEXT("override_material_count"), Mesh->OverrideMaterials.Num());
                    if (Mesh->SkeletalMesh)
                    {
                        USkeletalMesh* SkeletalMesh = Mesh->SkeletalMesh;
                        const FBoxSphereBounds Bounds = SkeletalMesh->GetBounds();
                        MeshObject->SetStringField(TEXT("unscaled_bounds_origin"), Bounds.Origin.ToString());
                        MeshObject->SetStringField(TEXT("unscaled_bounds_extent"), Bounds.BoxExtent.ToString());
                        MeshObject->SetNumberField(TEXT("unscaled_height"), Bounds.BoxExtent.Z * 2.0f);
                        MeshObject->SetNumberField(
                            TEXT("scaled_height"),
                            Bounds.BoxExtent.Z * 2.0f * Mesh->GetRelativeScale3D().Z);
                        TArray<TSharedPtr<FJsonValue>> Materials;
                        for (const FSkeletalMaterial& Material : SkeletalMesh->Materials)
                        {
                            Materials.Add(MakeShared<FJsonValueString>(
                                Material.MaterialInterface
                                    ? Material.MaterialInterface->GetPathName()
                                    : TEXT("")));
                        }
                        MeshObject->SetArrayField(TEXT("skeletal_mesh_materials"), Materials);
                        MeshObject->SetBoolField(
                            TEXT("has_vertex_colors"), SkeletalMesh->GetHasVertexColors());

                        TArray<TSharedPtr<FJsonValue>> MaterialSections;
                        bool bMaterialSectionIndicesValid = true;
                        FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
                        if (ImportedModel && ImportedModel->LODModels.IsValidIndex(0))
                        {
                            const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];
                            const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(0);
                            for (int32 SectionIndex = 0;
                                 SectionIndex < LODModel.Sections.Num();
                                 ++SectionIndex)
                            {
                                const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
                                int32 ResolvedMaterialIndex = Section.MaterialIndex;
                                if (LODInfo &&
                                    LODInfo->LODMaterialMap.IsValidIndex(SectionIndex) &&
                                    LODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
                                {
                                    ResolvedMaterialIndex = LODInfo->LODMaterialMap[SectionIndex];
                                }
                                const bool bIndexValid =
                                    SkeletalMesh->Materials.IsValidIndex(ResolvedMaterialIndex);
                                bMaterialSectionIndicesValid &= bIndexValid;

                                TSharedRef<FJsonObject> SectionObject = MakeShared<FJsonObject>();
                                SectionObject->SetNumberField(TEXT("section"), SectionIndex);
                                SectionObject->SetNumberField(
                                    TEXT("raw_material_index"), Section.MaterialIndex);
                                SectionObject->SetNumberField(
                                    TEXT("resolved_material_index"), ResolvedMaterialIndex);
                                SectionObject->SetBoolField(
                                    TEXT("material_index_valid"), bIndexValid);
                                SectionObject->SetNumberField(
                                    TEXT("triangles"), Section.NumTriangles);
                                SectionObject->SetNumberField(
                                    TEXT("vertices"), Section.NumVertices);
                                MaterialSections.Add(
                                    MakeShared<FJsonValueObject>(SectionObject));
                            }

                            TArray<FSoftSkinVertex> Vertices;
                            LODModel.GetVertices(Vertices);
                            TSet<FColor> UniqueColours;
                            FColor MinimumColour(255, 255, 255, 255);
                            FColor MaximumColour(0, 0, 0, 0);
                            for (const FSoftSkinVertex& Vertex : Vertices)
                            {
                                const FColor Colour = Vertex.Color;
                                UniqueColours.Add(Colour);
                                MinimumColour.R = FMath::Min(MinimumColour.R, Colour.R);
                                MinimumColour.G = FMath::Min(MinimumColour.G, Colour.G);
                                MinimumColour.B = FMath::Min(MinimumColour.B, Colour.B);
                                MinimumColour.A = FMath::Min(MinimumColour.A, Colour.A);
                                MaximumColour.R = FMath::Max(MaximumColour.R, Colour.R);
                                MaximumColour.G = FMath::Max(MaximumColour.G, Colour.G);
                                MaximumColour.B = FMath::Max(MaximumColour.B, Colour.B);
                                MaximumColour.A = FMath::Max(MaximumColour.A, Colour.A);
                            }

                            TSharedRef<FJsonObject> VertexColours = MakeShared<FJsonObject>();
                            VertexColours->SetNumberField(TEXT("count"), Vertices.Num());
                            VertexColours->SetNumberField(
                                TEXT("unique_rgba"), UniqueColours.Num());
                            TArray<TSharedPtr<FJsonValue>> MinimumValues;
                            TArray<TSharedPtr<FJsonValue>> MaximumValues;
                            const uint8 MinimumChannels[] = {
                                MinimumColour.R,
                                MinimumColour.G,
                                MinimumColour.B,
                                MinimumColour.A};
                            const uint8 MaximumChannels[] = {
                                MaximumColour.R,
                                MaximumColour.G,
                                MaximumColour.B,
                                MaximumColour.A};
                            for (uint8 Value : MinimumChannels)
                            {
                                MinimumValues.Add(MakeShared<FJsonValueNumber>(Value));
                            }
                            for (uint8 Value : MaximumChannels)
                            {
                                MaximumValues.Add(MakeShared<FJsonValueNumber>(Value));
                            }
                            VertexColours->SetArrayField(
                                TEXT("minimum_rgba"), MinimumValues);
                            VertexColours->SetArrayField(
                                TEXT("maximum_rgba"), MaximumValues);
                            MeshObject->SetObjectField(
                                TEXT("vertex_colours"), VertexColours);
                        }
                        MeshObject->SetArrayField(
                            TEXT("material_sections"), MaterialSections);
                        MeshObject->SetBoolField(
                            TEXT("material_section_indices_valid"),
                            bMaterialSectionIndicesValid && MaterialSections.Num() > 0);
                    }
                    Root->SetObjectField(TEXT("mesh"), MeshObject);
                }
            }
        }
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

FString UMMAEditorAnimationLibrary::DescribeClassFunctions(UClass* Class)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("class"), Class ? Class->GetPathName() : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Functions;
    if (Class)
    {
        for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper);
             FunctionIt; ++FunctionIt)
        {
            UFunction* Function = *FunctionIt;
            if (!Function)
            {
                continue;
            }
            TSharedRef<FJsonObject> FunctionObject = MakeShared<FJsonObject>();
            FunctionObject->SetStringField(TEXT("name"), Function->GetName());
            FunctionObject->SetNumberField(TEXT("parameter_size"), Function->ParmsSize);
            TArray<TSharedPtr<FJsonValue>> Parameters;
            for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
            {
                FProperty* Property = *PropertyIt;
                if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
                {
                    continue;
                }
                TSharedRef<FJsonObject> Parameter = MakeShared<FJsonObject>();
                Parameter->SetStringField(TEXT("name"), Property->GetName());
                Parameter->SetStringField(TEXT("type"), Property->GetClass()->GetName());
                Parameter->SetBoolField(
                    TEXT("return"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
                Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
            }
            FunctionObject->SetArrayField(TEXT("parameters"), Parameters);
            Functions.Add(MakeShared<FJsonValueObject>(FunctionObject));
        }
    }
    Root->SetArrayField(TEXT("functions"), Functions);
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
