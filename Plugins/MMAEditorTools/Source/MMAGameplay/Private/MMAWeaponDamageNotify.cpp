#include "MMAWeaponDamageNotify.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace
{
FString NormalizedComponentName(const FString& Value)
{
    FString Result = Value.ToLower();
    Result.ReplaceInline(TEXT("_gen_variable"), TEXT(""));
    Result.ReplaceInline(TEXT("_"), TEXT(""));
    Result.ReplaceInline(TEXT(" "), TEXT(""));
    return Result;
}
}

FString UMMAWeaponDamageNotify::GetNotifyName_Implementation() const
{
    return TEXT("MMA Weapon Damage");
}

UPrimitiveComponent* UMMAWeaponDamageNotify::FindWeaponHitbox(AActor* Owner) const
{
    if (!Owner)
    {
        return nullptr;
    }

    const FString Wanted = NormalizedComponentName(WeaponComponentName);
    TArray<UPrimitiveComponent*> Components;
    Owner->GetComponents(Components);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component)
        {
            continue;
        }
        const FString Candidate = NormalizedComponentName(Component->GetName());
        if (Candidate == Wanted || Candidate.Contains(Wanted))
        {
            return Component;
        }
    }
    return nullptr;
}

bool UMMAWeaponDamageNotify::InvokeDamageCheck(
    AActor* Owner,
    UPrimitiveComponent* OtherComponent) const
{
    if (!Owner || !OtherComponent || !OtherComponent->GetOwner())
    {
        return false;
    }

    UFunction* Function = Owner->FindFunction(TEXT("Damage Check"));
    if (!Function)
    {
        Function = Owner->FindFunction(TEXT("Damage_Check"));
    }
    if (!Function)
    {
        return false;
    }

    TArray<uint8> Parameters;
    Parameters.SetNumZeroed(Function->ParmsSize);
    Function->InitializeStruct(Parameters.GetData());

    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property->HasAnyPropertyFlags(CPF_Parm) ||
            Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            continue;
        }

        FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
        if (!ObjectProperty)
        {
            continue;
        }

        UObject* Value = nullptr;
        if (ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()))
        {
            Value = OtherComponent->GetOwner();
        }
        else if (ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
        {
            Value = OtherComponent;
        }
        ObjectProperty->SetObjectPropertyValue_InContainer(Parameters.GetData(), Value);
    }

    Owner->ProcessEvent(Function, Parameters.GetData());
    Function->DestroyStruct(Parameters.GetData());
    return true;
}

void UMMAWeaponDamageNotify::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation)
{
    Super::Notify(MeshComp, Animation);
    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    UPrimitiveComponent* Hitbox = FindWeaponHitbox(Owner);
    if (!Hitbox)
    {
        return;
    }

    Hitbox->UpdateOverlaps();
    TArray<UPrimitiveComponent*> OverlappingComponents;
    Hitbox->GetOverlappingComponents(OverlappingComponents);
    for (UPrimitiveComponent* OtherComponent : OverlappingComponents)
    {
        InvokeDamageCheck(Owner, OtherComponent);
    }
}
