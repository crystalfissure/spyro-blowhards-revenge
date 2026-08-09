#include "MMAEditorAnimationLibrary.h"

#include "MMAChaseLeashComponent.h"
#include "MMAHedgeTrimmerBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
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

bool UMMAEditorAnimationLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
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
                    Contract->SetStringField(TEXT("default_drop"), AssetPath(Behavior->DefaultDropClass.Get()));
                    Contract->SetNumberField(TEXT("detection_radius"), Behavior->DetectionRadius);
                    Contract->SetNumberField(TEXT("lose_interest_radius"), Behavior->LoseInterestRadius);
                    Contract->SetNumberField(TEXT("maximum_distance_from_home"), Behavior->MaximumDistanceFromHome);
                    Contract->SetNumberField(TEXT("attack_range"), Behavior->AttackRange);
                    Contract->SetNumberField(TEXT("attack_hit_range"), Behavior->AttackHitRange);
                    Contract->SetNumberField(TEXT("attack_contact_seconds"), Behavior->AttackContactSeconds);
                    Contract->SetNumberField(TEXT("attack_cooldown_seconds"), Behavior->AttackCooldownSeconds);
                    Contract->SetNumberField(TEXT("recoil_horizontal_speed"), Behavior->RecoilHorizontalSpeed);
                    Contract->SetNumberField(TEXT("recoil_vertical_speed"), Behavior->RecoilVerticalSpeed);
                    Contract->SetNumberField(TEXT("death_poof_padding_seconds"), Behavior->DeathPoofPaddingSeconds);
                    Contract->SetBoolField(TEXT("debug_messages"), Behavior->bEnableDebugMessages);
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
                        const FBoxSphereBounds Bounds = Mesh->SkeletalMesh->GetBounds();
                        MeshObject->SetStringField(TEXT("unscaled_bounds_origin"), Bounds.Origin.ToString());
                        MeshObject->SetStringField(TEXT("unscaled_bounds_extent"), Bounds.BoxExtent.ToString());
                        MeshObject->SetNumberField(TEXT("unscaled_height"), Bounds.BoxExtent.Z * 2.0f);
                        MeshObject->SetNumberField(
                            TEXT("scaled_height"),
                            Bounds.BoxExtent.Z * 2.0f * Mesh->GetRelativeScale3D().Z);
                        TArray<TSharedPtr<FJsonValue>> Materials;
                        for (const FSkeletalMaterial& Material : Mesh->SkeletalMesh->Materials)
                        {
                            Materials.Add(MakeShared<FJsonValueString>(
                                Material.MaterialInterface
                                    ? Material.MaterialInterface->GetPathName()
                                    : TEXT("")));
                        }
                        MeshObject->SetArrayField(TEXT("skeletal_mesh_materials"), Materials);
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
