#include "MMAEditorAnimationLibrary.h"

#include "MMAChaseLeashComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
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
