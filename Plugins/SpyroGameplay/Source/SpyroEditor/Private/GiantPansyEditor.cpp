#include "SpyroEditorLibrary.h"

#include "GiantPansyBehaviorComponent.h"
#include "GiantPansyAnimInstance.h"
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
#include "Components/SplineComponent.h"
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

bool USpyroEditorLibrary::ConfigureGiantPansy(UBlueprint* Blueprint,const TArray<USkeletalMesh*>& Meshes,const TArray<UAnimSequence*>& Animations,const TArray<USoundBase*>& Sounds,USoundAttenuation* Attenuation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || Meshes.Num()!=2 || Meshes.Contains(nullptr) || Sounds.Num()!=2 || Sounds.Contains(nullptr) || Animations.Num()!=7 || Animations.Contains(nullptr)) return false;
    USkeletalMesh* Mesh=Meshes[0];
    auto* CDO=Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO) return false;
    Blueprint->Modify(); CDO->Modify();
    auto* SCS=Blueprint->SimpleConstructionScript;
    const FName BehaviorName(TEXT("GiantPansyBehavior"));
    auto* Node=SCS->FindSCSNode(BehaviorName);
    if (!Node) { Node=SCS->CreateNode(UGiantPansyBehaviorComponent::StaticClass(),BehaviorName); SCS->AddNode(Node); }
    auto* Behavior=Cast<UGiantPansyBehaviorComponent>(Node->ComponentTemplate);
    Behavior->Meshes=Meshes; Behavior->Animations=Animations; Behavior->OriginalSounds=Sounds; Behavior->SoundAttenuation=Attenuation;
    auto* Skel=CDO->GetMesh(); Skel->Modify(); Skel->SetSkeletalMesh(Mesh);
    // Base_Enemy_BP carries an untextured slot override. Use this enemy's mesh
    // materials so Blueprint instances match skeletal-mesh/animation previews.
    Skel->EmptyOverrideMaterials();
    const float Scale=.584416f;
    const float H=100.f, R=47.f;
    Skel->SetRelativeScale3D(FVector(Scale));
    Skel->SetRelativeLocation(FVector(0,0,-H));
    Skel->SetRelativeRotation(FRotator(0,-90,0));
    Skel->SetAnimInstanceClass(UGiantPansyAnimInstance::StaticClass());
    Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Capsule=CDO->GetCapsuleComponent(); Capsule->Modify(); Capsule->SetCapsuleSize(R,H);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Overlap); Capsule->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Capsule->CanCharacterStepUpOn=ECB_No;
    for (bool Sensor:{false,true})
    {
        const FName Name(Sensor?TEXT("GiantPansyChargeSensor"):TEXT("GiantPansyBodyCollision"));
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
    UClass* Gem=LoadClass<AActor>(nullptr,TEXT("/Game/SpyroContent/Global_Assets/Global_Level_Items/Gems/Actors/Child_Actors/Gem_Blueish_Purple_BP.Gem_Blueish_Purple_BP_C"));
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
            if (auto* Resists=FindFProperty<FArrayProperty>(Template->GetClass(),TEXT("Damage_Resistances")))
            {
                auto* Byte=CastField<FByteProperty>(Resists->Inner); if (!Byte) return false;
                FScriptArrayHelper Values(Resists,Resists->ContainerPtrToValuePtr<void>(Template));
                bool HasRam=false; for (int32 I=0; I<Values.Num(); ++I) HasRam |= Byte->GetPropertyValue(Values.GetRawPtr(I))==5;
                if (!HasRam) Byte->SetPropertyValue(Values.GetRawPtr(Values.AddValue()),5);
            }
            if (auto* Items=FindFProperty<FArrayProperty>(Template->GetClass(),TEXT("Items_to_Drop")))
            { auto* Inner=CastField<FObjectPropertyBase>(Items->Inner); if (!Inner) return false; FScriptArrayHelper A(Items,Items->ContainerPtrToValuePtr<void>(Template)); A.EmptyValues(); A.AddValue(); Inner->SetObjectPropertyValue(A.GetRawPtr(0),Gem); }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); Blueprint->MarkPackageDirty(); return true;
}

