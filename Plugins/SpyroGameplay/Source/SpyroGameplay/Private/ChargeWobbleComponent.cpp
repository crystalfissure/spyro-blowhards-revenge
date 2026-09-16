#include "ChargeWobbleComponent.h"

#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
    FString NormalizedPropertyName(const FProperty* Property)
    {
        FString Name = Property->GetName().ToLower();
        Name.ReplaceInline(TEXT(" "), TEXT(""));
        Name.ReplaceInline(TEXT("_"), TEXT(""));
        Name.ReplaceInline(TEXT("'"), TEXT(""));
        return Name;
    }
}

UChargeWobbleComponent::UChargeWobbleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    static ConstructorHelpers::FClassFinder<UActorComponent> DamageClass(
        TEXT("/Game/SpyroContent/Global_Assets/Global_Components/Damageable_Com"));
    if (DamageClass.Succeeded())
    {
        DamageableClass = DamageClass.Class;
    }
}

USkeletalMeshComponent* UChargeWobbleComponent::ResolveMesh() const
{
    AActor* Owner = GetOwner();
    return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UChargeWobbleComponent::OnRegister()
{
    Super::OnRegister();
    Mesh = ResolveMesh();
    RestoreRestPose();
}

void UChargeWobbleComponent::BeginPlay()
{
    Super::BeginPlay();
    Mesh = ResolveMesh();
    EnsureHitbox();
    EnsureDamageable();
    ConfigureDamageContract();
    RestoreRestPose();
}

void UChargeWobbleComponent::EnsureHitbox()
{
    AActor* Owner = GetOwner();
    Mesh = ResolveMesh();
    if (!Owner || !Mesh)
    {
        return;
    }

    if (Hitbox && IsValid(Hitbox) && Hitbox->GetOwner() == Owner)
    {
        return;
    }

    TArray<UBoxComponent*> Boxes;
    Owner->GetComponents<UBoxComponent>(Boxes);
    for (UBoxComponent* Box : Boxes)
    {
        if (Box && Box->GetFName() == TEXT("ChargeWobbleHitbox"))
        {
            Hitbox = Box;
            break;
        }
    }

    if (!Hitbox)
    {
        Hitbox = NewObject<UBoxComponent>(Owner, TEXT("ChargeWobbleHitbox"));
        Hitbox->SetupAttachment(Mesh);
        Hitbox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Hitbox->SetGenerateOverlapEvents(true);
        Hitbox->RegisterComponent();
    }

    if (Mesh->SkeletalMesh)
    {
        const FBoxSphereBounds Bounds = Mesh->SkeletalMesh->GetBounds();
        Hitbox->SetRelativeLocation(Bounds.Origin);
        Hitbox->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(1.f)));
    }
}

void UChargeWobbleComponent::EnsureDamageable()
{
    AActor* Owner = GetOwner();
    if (!Owner || !DamageableClass)
    {
        return;
    }

    if (Damageable && IsValid(Damageable) && Damageable->GetOwner() == Owner)
    {
        return;
    }

    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component && Component->IsA(DamageableClass))
        {
            Damageable = Component;
            return;
        }
    }

    Damageable = NewObject<UActorComponent>(Owner, DamageableClass, TEXT("Damageable_Com"));
    Damageable->RegisterComponent();
}

void UChargeWobbleComponent::ConfigureDamageContract()
{
    bDamageContractReady = false;
    if (!Damageable || !Hitbox)
    {
        return;
    }

    FObjectPropertyBase* BoxProperty = nullptr;
    FMulticastDelegateProperty* HitDelegate = nullptr;
    for (TFieldIterator<FProperty> It(Damageable->GetClass()); It; ++It)
    {
        const FString Name = NormalizedPropertyName(*It);
        if (Name == TEXT("objectshitboxcomponent"))
        {
            BoxProperty = CastField<FObjectPropertyBase>(*It);
        }
        if (Name == TEXT("damagewasattempted"))
        {
            HitDelegate = CastField<FMulticastDelegateProperty>(*It);
        }
    }

    if (!BoxProperty || !HitDelegate || !Hitbox->IsA(BoxProperty->PropertyClass) ||
        !HitDelegate->SignatureFunction || HitDelegate->SignatureFunction->NumParms != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Charge wobble hit contract does not match Damageable_Com"));
        return;
    }

    BoxProperty->SetObjectPropertyValue_InContainer(Damageable, Hitbox);
    ResistIncomingHits();
    FScriptDelegate Callback;
    Callback.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UChargeWobbleComponent, ReactToAttack));
    HitDelegate->AddDelegate(Callback, Damageable);
    bDamageContractReady = true;
}

void UChargeWobbleComponent::ResistIncomingHits()
{
    if (!Damageable)
    {
        return;
    }

    FArrayProperty* Resistances = nullptr;
    for (TFieldIterator<FProperty> It(Damageable->GetClass()); It; ++It)
    {
        const FString Name = NormalizedPropertyName(*It);
        if (Name == TEXT("damageresistances"))
        {
            Resistances = CastField<FArrayProperty>(*It);
            break;
        }
    }

    if (!Resistances || !CastField<FByteProperty>(Resistances->Inner))
    {
        return;
    }

    FScriptArrayHelper Helper(Resistances, Resistances->ContainerPtrToValuePtr<void>(Damageable));
    auto Contains = [&Helper](uint8 Value)
    {
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            if (*reinterpret_cast<uint8*>(Helper.GetRawPtr(Index)) == Value)
            {
                return true;
            }
        }
        return false;
    };
    auto AddUnique = [&Helper, &Contains](uint8 Value)
    {
        if (!Contains(Value))
        {
            const int32 Index = Helper.AddValue();
            *reinterpret_cast<uint8*>(Helper.GetRawPtr(Index)) = Value;
        }
    };

    TArray<uint8> Values;
    if (UEnum* DamageTypes = LoadObject<UEnum>(nullptr,
        TEXT("/Game/SpyroContent/Global_Assets/Global_Characters/Damage_Types.Damage_Types")))
    {
        for (int32 Index = 0; Index < DamageTypes->NumEnums(); ++Index)
        {
            if (DamageTypes->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
            {
                continue;
            }
            Values.AddUnique(static_cast<uint8>(DamageTypes->GetValueByIndex(Index)));
        }
    }
    if (!Values.Num())
    {
        Values.Add(2);
        Values.Add(5);
    }
    for (uint8 Value : Values)
    {
        AddUnique(Value);
    }
}

void UChargeWobbleComponent::RestoreRestPose()
{
    bReacting = false;
    ReactionElapsed = 0.f;
    SetComponentTickEnabled(false);
    Mesh = ResolveMesh();
    if (RestAnimation && Mesh && Mesh->SkeletalMesh &&
        RestAnimation->GetSkeleton() == Mesh->SkeletalMesh->GetSkeleton())
    {
        Mesh->PlayAnimation(RestAnimation, false);
        Mesh->SetPosition(0.f, false);
        Mesh->Stop();
    }
}

void UChargeWobbleComponent::ReactToAttack()
{
    Mesh = ResolveMesh();
    if (bReacting || !ReactionAnimation || !Mesh || !Mesh->SkeletalMesh ||
        ReactionAnimation->GetSkeleton() != Mesh->SkeletalMesh->GetSkeleton())
    {
        return;
    }
    ReactionElapsed = 0.f;
    bReacting = true;
    Mesh->PlayAnimation(ReactionAnimation, false);
    SetComponentTickEnabled(true);
}

void UChargeWobbleComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bReacting)
    {
        return;
    }
    ReactionElapsed += FMath::Max(DeltaTime, 0.f);
    if (!ReactionAnimation || ReactionElapsed >= ReactionAnimation->GetPlayLength())
    {
        RestoreRestPose();
    }
}
