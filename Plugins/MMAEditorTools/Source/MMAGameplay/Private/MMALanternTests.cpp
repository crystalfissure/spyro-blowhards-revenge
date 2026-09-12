#if WITH_DEV_AUTOMATION_TESTS
#include "MMALantern.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMALanternTest,"Spyro.Props.DarkHollowLantern",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMMALanternTest::RunTest(const FString& Parameters)
{
    UClass* Class=LoadClass<AMMALantern>(nullptr,TEXT("/Game/OT_Ports/S1/S1_Objects/Home_00_Artisans/02_DarkHollow/Interactive_Lantern/BP_DarkHollowLantern.BP_DarkHollowLantern_C"));
    if(!TestNotNull(TEXT("Production Blueprint loads"),Class)) return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    AMMALantern* Lantern=World->SpawnActor<AMMALantern>(Class);
    if(!TestNotNull(TEXT("Lantern spawns"),Lantern)) return false;
    TestTrue(TEXT("Damage callback and hitbox bind"),Lantern->bDamageContractReady);
    TestFalse(TEXT("Starts at rest"),Lantern->bReacting);
    TestFalse(TEXT("Idle actor does not tick"),Lantern->IsActorTickEnabled());
    TestTrue(TEXT("Reaction uses original skeleton"),Lantern->ReactionAnimation && Lantern->ReactionAnimation->GetSkeleton()==Lantern->LanternMesh->SkeletalMesh->GetSkeleton());
    TestTrue(TEXT("Hitbox has volume"),Lantern->Hitbox->GetUnscaledBoxExtent().GetMin()>0.f);
    UFunction* Deal=Lantern->Damageable->FindFunction(TEXT("Deal Damage"));
    if(TestNotNull(TEXT("Project attack entry point exists"),Deal))
    {
        FByteProperty* Type=FindFProperty<FByteProperty>(Deal,TEXT("Damage Type"));
        UEnum* Enum=Type ? Type->Enum : nullptr;
        int32 AttacksTested=0;
        if(TestNotNull(TEXT("Project damage enum"),Enum))
        for(int32 Index=0;Index<Enum->NumEnums()-1;++Index)
        {
            const FString Name=Enum->GetDisplayNameTextByIndex(Index).ToString();
            if(Name!=TEXT("Ram") && Name!=TEXT("Burn")) continue;
            TArray<uint8> Buffer; Buffer.SetNumZeroed(Deal->ParmsSize);
            Type->SetPropertyValue_InContainer(Buffer.GetData(),uint8(Enum->GetValueByIndex(Index)));
            if(FObjectPropertyBase* Person=FindFProperty<FObjectPropertyBase>(Deal,TEXT("Person Who Dealt the Damage")))
                Person->SetObjectPropertyValue_InContainer(Buffer.GetData(),Lantern);
            Lantern->Damageable->ProcessEvent(Deal,Buffer.GetData());
            TestTrue(*FString::Printf(TEXT("%s triggers reaction"),*Name),Lantern->bReacting);
            Lantern->Tick(1.f);
            Lantern->Damageable->ProcessEvent(Deal,Buffer.GetData());
            Lantern->Tick(Lantern->ReactionAnimation->GetPlayLength()-1.f+0.01f);
            TestFalse(TEXT("Duplicate hit does not restart; returns to rest"),Lantern->bReacting);
            TestFalse(TEXT("Stops actor ticking after reaction"),Lantern->IsActorTickEnabled());
            TestFalse(TEXT("Prop survives attack"),Lantern->IsActorBeingDestroyed());
            ++AttacksTested;
        }
        TestTrue(TEXT("Charge and flame attack types exercised"),AttacksTested>=2);
    }
    // Sample animated bones in Unreal, ensuring finite transforms and no root scale jump.
    Lantern->ReactToAttack();
    const FTransform ReferenceRoot=Lantern->LanternMesh->SkeletalMesh->GetRefSkeleton().GetRefBonePose()[0];
    for(float Time : {0.f,0.5f,1.f,2.f,3.8f})
    {
        Lantern->LanternMesh->SetPosition(Time,false);
        Lantern->LanternMesh->TickAnimation(0.f,false);
        Lantern->LanternMesh->RefreshBoneTransforms();
        const auto& Transforms=Lantern->LanternMesh->GetComponentSpaceTransforms();
        TestTrue(TEXT("Bone transforms evaluated"),Transforms.Num()>0);
        for(const FTransform& T:Transforms) TestFalse(TEXT("Animated transforms are finite"),T.ContainsNaN());
        if(Transforms.Num()) TestTrue(*FString::Printf(TEXT("Imported root matches reference (actual %s; reference %s)"),*Transforms[0].ToHumanReadableString(),*ReferenceRoot.ToHumanReadableString()),Transforms[0].Equals(ReferenceRoot,0.01f));
    }
    Lantern->Destroy();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
