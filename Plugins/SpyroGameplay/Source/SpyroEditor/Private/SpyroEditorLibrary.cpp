#include "SpyroEditorLibrary.h"

#include "GnorcThiefBehaviorComponent.h"
#include "TownSquareEnemyBehaviorComponent.h"
#include "TownSquareEnemyAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/BoxComponent.h"
#include "Components/ActorComponent.h"
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
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

bool USpyroEditorLibrary::MountTownSquareTestContent(const FString& Directory)
{
    if (!FPaths::DirectoryExists(Directory)) return false;
    FString Path=FPaths::ConvertRelativePathToFull(Directory); FPaths::NormalizeDirectoryName(Path);
    FPackageName::RegisterMountPoint(TEXT("/TownSquareTests/"),Path+TEXT("/")); return true;
}
FString USpyroEditorLibrary::DescribeTownSquareDamageTypes()
{
    auto* E=LoadObject<UEnum>(nullptr,TEXT("/Game/SpyroContent/Global_Assets/Global_Characters/Damage_Types.Damage_Types"));
    FString Out;
    if (E) for (int32 I=0;I<E->NumEnums();++I) Out+=FString::Printf(TEXT("%lld=%s\n"),E->GetValueByIndex(I),*E->GetDisplayNameTextByIndex(I).ToString());
    return Out;
}
bool USpyroEditorLibrary::PrepareTownSquareTest(AActor* Actor)
{
    if (!Actor || !Actor->GetWorld() || Actor->GetWorld()->WorldType!=EWorldType::PIE || !Actor->GetWorld()->GetMapName().Contains(TEXT("Bull_Toreador_Test"))) return false;
    auto* Instance=Actor->GetWorld()->GetGameInstance();
    if (auto* Name=Instance?FindFProperty<FStrProperty>(Instance->GetClass(),TEXT("Current_Level_Name")):nullptr)
    {
        const FString Slot(TEXT("Bull_Toreador_Automation_Only")); Name->SetPropertyValue_InContainer(Instance,Slot);
        auto* Save=LoadClass<USaveGame>(nullptr,TEXT("/Game/SpyroContent/Global_Assets/Global_SaveData/Individual_Level_SaveData.Individual_Level_SaveData_C"));
        if (!Save) return false;
        if (!UGameplayStatics::DoesSaveGameExist(Slot,0)) UGameplayStatics::SaveGameToSlot(UGameplayStatics::CreateSaveGameObject(Save),Slot,0);
    }
    TArray<UActorComponent*> Components;Actor->GetComponents(Components);
    for (auto* C:Components) if (C->GetClass()->GetName()==TEXT("Drops_Items_C"))
        if (auto* P=FindFProperty<FBoolProperty>(C->GetClass(),TEXT("Safe to Destroy"))) { P->SetPropertyValue_InContainer(C,true);return true; }
    return false;
}
bool USpyroEditorLibrary::PrepareTownSquareChargeTest(AActor* Actor)
{
    auto* Player=Cast<ACharacter>(Actor);
    if (!Player || !Player->GetWorld() || Player->GetWorld()->WorldType!=EWorldType::PIE || !Player->GetWorld()->GetMapName().Contains(TEXT("Bull_Toreador_Test")) || Player->GetClass()->GetName()!=TEXT("BP_Spyro_C")) return false;
    auto* P=FindFProperty<FByteProperty>(Player->GetClass(),TEXT("Player_State"));
    if (!P || !P->Enum) return false;
    const int64 Charging=P->Enum->GetValueByNameString(TEXT("NewEnumerator4")); if (Charging==INDEX_NONE) return false;
    P->SetPropertyValue_InContainer(Player,uint8(Charging)); Player->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel4); return true;
}

bool USpyroEditorLibrary::ConfigureTownSquareEnemy(UBlueprint* Blueprint,bool Bull,USkeletalMesh* Mesh,const TArray<UAnimSequence*>& Animations,const TArray<USoundBase*>& Sounds,USoundAttenuation* Attenuation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !Mesh || Animations.Num()!=10 || Animations.Contains(nullptr)) return false;
    auto* CDO=Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO) return false;
    Blueprint->Modify(); CDO->Modify();
    auto* SCS=Blueprint->SimpleConstructionScript;
    const FName BehaviorName(Bull?TEXT("BullBehavior"):TEXT("ToreadorBehavior"));
    auto* Node=SCS->FindSCSNode(BehaviorName);
    if (!Node) { Node=SCS->CreateNode(Bull?UBullBehaviorComponent::StaticClass():UToreadorBehaviorComponent::StaticClass(),BehaviorName); SCS->AddNode(Node); }
    auto* Behavior=Cast<UTownSquareEnemyBehaviorComponent>(Node->ComponentTemplate);
    Behavior->Animations=Animations; Behavior->OriginalSounds=Sounds; Behavior->SoundAttenuation=Attenuation;
    auto* Skel=CDO->GetMesh(); Skel->Modify(); Skel->SetSkeletalMesh(Mesh);
    // Base_Enemy_BP carries an untextured slot override. Use this enemy's mesh
    // materials so Blueprint instances match skeletal-mesh/animation previews.
    Skel->EmptyOverrideMaterials();
    const float Scale=Bull?.292208f:.584416f;
    const float H=90.f, R=Bull?70.f:60.f;
    Skel->SetRelativeScale3D(FVector(Scale));
    Skel->SetRelativeLocation(FVector(0,0,-H-(Mesh->GetBounds().Origin.Z-Mesh->GetBounds().BoxExtent.Z)*Scale));
    Skel->SetRelativeRotation(FRotator(0,-90,0));
    Skel->SetAnimInstanceClass(Bull?UBullAnimInstance::StaticClass():UToreadorAnimInstance::StaticClass());
    Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Capsule=CDO->GetCapsuleComponent(); Capsule->Modify(); Capsule->SetCapsuleSize(R,H);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Overlap); Capsule->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Capsule->CanCharacterStepUpOn=ECB_No;
    for (bool Sensor:{false,true})
    {
        const FName Name(Sensor?TEXT("TownSquareChargeSensor"):TEXT("TownSquareBodyCollision"));
        auto* BoxNode=SCS->FindSCSNode(Name);
        if (!BoxNode) { BoxNode=SCS->CreateNode(UBoxComponent::StaticClass(),Name); SCS->AddNode(BoxNode); }
        auto* Box=Cast<UBoxComponent>(BoxNode->ComponentTemplate);
        Box->SetBoxExtent(FVector(R+12+(Sensor?12:0),R+12+(Sensor?12:0),H-2));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldDynamic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4,Sensor?ECR_Overlap:ECR_Block);
        if (!Sensor) Box->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
        Box->SetGenerateOverlapEvents(Sensor); Box->CanCharacterStepUpOn=ECB_No;
        Box->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable,0)); Box->SetCanEverAffectNavigation(false);
    }
    // Defaults participate in existing level gem counting; instance overrides remain editable.
    auto* Handler=Blueprint->GetInheritableComponentHandler(true);
    UClass* Gem=LoadClass<AActor>(nullptr,TEXT("/Game/SpyroContent/Global_Assets/Global_Level_Items/Gems/Actors/Child_Actors/Gem_Green_BP.Gem_Green_BP_C"));
    if (!Handler || !Gem) return false;
    for (UClass* Parent=Blueprint->ParentClass;Parent;Parent=Parent->GetSuperClass())
    {
        auto* G=Cast<UBlueprintGeneratedClass>(Parent); if (!G || !G->SimpleConstructionScript) continue;
        for (auto* N:G->SimpleConstructionScript->GetAllNodes())
        {
            if (!N || (!N->ComponentClass->GetName().Contains(TEXT("Damageable_Com")) && !N->ComponentClass->GetName().Contains(TEXT("Drops_Items")))) continue;
            const FComponentKey Key(N); auto* Template=Handler->GetOverridenComponentTemplate(Key);
            if (!Template) Template=Handler->CreateOverridenComponentTemplate(Key);
            if (!Template) return false; Template->Modify();
            if (auto* HP=FindFProperty<FIntProperty>(Template->GetClass(),TEXT("Hit Points"))) HP->SetPropertyValue_InContainer(Template,1);
            if (auto* Items=FindFProperty<FArrayProperty>(Template->GetClass(),TEXT("Items_to_Drop")))
            { auto* Inner=CastField<FObjectPropertyBase>(Items->Inner); if (!Inner) return false; FScriptArrayHelper A(Items,Items->ContainerPtrToValuePtr<void>(Template)); A.EmptyValues(); A.AddValue(); Inner->SetObjectPropertyValue(A.GetRawPtr(0),Gem); }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); Blueprint->MarkPackageDirty(); return true;
}

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
        (!Actor->GetWorld()->GetMapName().Contains(TEXT("Gnorc_Thief_Test")) &&
         !Actor->ActorHasTag(TEXT("GnorcThief_Automation_Only"))) ||
        !Actor->FindComponentByClass<UGnorcThiefBehaviorComponent>()) return false;
    UGameInstance* Instance = Actor->GetWorld()->GetGameInstance();
    // Offscreen simulation otherwise leaves the editor's audio device active/muted.
    // Explicit tagging also permits isolated tests against the real level terrain.
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

bool USpyroEditorLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}

#if 0
// Historical one-shot lantern migration implementation. The production
// lanterns now own this behavior entirely in /Game Blueprint assets, and the
// public editor API has been removed. Retained disabled only as migration
// provenance until the docs-workspace archive is consolidated.
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
#endif
