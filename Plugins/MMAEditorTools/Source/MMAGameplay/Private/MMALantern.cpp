#include "MMALantern.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AMMALantern::AMMALantern()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    LanternMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LanternMesh"));
    RootComponent=LanternMesh;
    LanternMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LanternMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Hitbox=CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
    Hitbox->SetupAttachment(LanternMesh);
    Hitbox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Hitbox->SetGenerateOverlapEvents(true);
    static ConstructorHelpers::FClassFinder<UActorComponent> DamageClass(TEXT("/Game/SpyroContent/Global_Assets/Global_Components/Damageable_Com"));
    if(DamageClass.Succeeded())
        Damageable=Cast<UActorComponent>(CreateDefaultSubobject(TEXT("Damageable"),DamageClass.Class,DamageClass.Class,true,false));
}

void AMMALantern::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if(LanternMesh->SkeletalMesh)
    {
        const FBoxSphereBounds Bounds=LanternMesh->SkeletalMesh->GetBounds();
        Hitbox->SetRelativeLocation(Bounds.Origin);
        Hitbox->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(1.f)));
    }
    RestoreRestPose();
}

void AMMALantern::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    ConfigureDamageContract();
}

void AMMALantern::ConfigureDamageContract()
{
    bDamageContractReady=false;
    if(!Damageable) return;
    FObjectPropertyBase* BoxProperty=nullptr;
    FMulticastDelegateProperty* AttackDelegate=nullptr;
    for(TFieldIterator<FProperty> It(Damageable->GetClass());It;++It)
    {
        FString Name=It->GetName().ToLower();
        Name.ReplaceInline(TEXT(" "),TEXT(""));
        Name.ReplaceInline(TEXT("_"),TEXT(""));
        Name.ReplaceInline(TEXT("'"),TEXT(""));
        if(Name==TEXT("objectshitboxcomponent")) BoxProperty=CastField<FObjectPropertyBase>(*It);
        if(Name==TEXT("calldealdamage")) AttackDelegate=CastField<FMulticastDelegateProperty>(*It);
    }
    if(!BoxProperty || !AttackDelegate || !Hitbox->IsA(BoxProperty->PropertyClass) || AttackDelegate->SignatureFunction->NumParms!=0)
    {
        UE_LOG(LogTemp,Error,TEXT("Lantern damage contract does not match Damageable_Com"));
        return;
    }
    BoxProperty->SetObjectPropertyValue_InContainer(Damageable,Hitbox);
    FScriptDelegate Callback;
    Callback.BindUFunction(this,GET_FUNCTION_NAME_CHECKED(AMMALantern,ReactToAttack));
    AttackDelegate->AddDelegate(Callback,Damageable);
    bDamageContractReady=true;
}

void AMMALantern::BeginPlay()
{
    Super::BeginPlay();
    RestoreRestPose();
}

void AMMALantern::RestoreRestPose()
{
    bReacting=false;
    ReactionElapsed=0.f;
    SetActorTickEnabled(false);
    if(RestAnimation && LanternMesh->SkeletalMesh && RestAnimation->GetSkeleton()==LanternMesh->SkeletalMesh->GetSkeleton())
    {
        LanternMesh->PlayAnimation(RestAnimation,false);
        LanternMesh->SetPosition(0.f,false);
        LanternMesh->Stop();
    }
}

void AMMALantern::ReactToAttack()
{
    if(bReacting || !ReactionAnimation || !LanternMesh->SkeletalMesh || ReactionAnimation->GetSkeleton()!=LanternMesh->SkeletalMesh->GetSkeleton()) return;
    ReactionElapsed=0.f;
    bReacting=true;
    LanternMesh->PlayAnimation(ReactionAnimation,false);
    SetActorTickEnabled(true);
}

void AMMALantern::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bReacting) return;
    ReactionElapsed+=FMath::Max(DeltaSeconds,0.f);
    if(!ReactionAnimation || ReactionElapsed>=ReactionAnimation->GetPlayLength()) RestoreRestPose();
}
