#include "GnorcThiefBehaviorComponent.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace GnorcThief
{
FString Key(FString Name)
{
    Name = Name.ToLower();
    Name.ReplaceInline(TEXT(" "), TEXT(""));
    Name.ReplaceInline(TEXT("_"), TEXT(""));
    Name.ReplaceInline(TEXT("'"), TEXT(""));
    return Name;
}
FProperty* Property(UObject* Object, const TCHAR* Name)
{
    if (Object) for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        if (Key(It->GetName()) == Key(Name)) return *It;
    return nullptr;
}
bool Bool(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FBoolProperty>(Property(Object, Name));
    return P && P->GetPropertyValue_InContainer(Object);
}
void SetBool(UObject* Object, const TCHAR* Name, bool Value)
{
    if (auto* P = CastField<FBoolProperty>(Property(Object, Name))) P->SetPropertyValue_InContainer(Object, Value);
}
int32 Number(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FNumericProperty>(Property(Object, Name));
    if (!P) return 0;
    const void* V = P->ContainerPtrToValuePtr<void>(Object);
    return P->IsInteger() ? int32(P->GetSignedIntPropertyValue(V)) : int32(P->GetFloatingPointPropertyValue(V));
}
void SetNumber(UObject* Object, const TCHAR* Name, double Value)
{
    if (auto* P = CastField<FNumericProperty>(Property(Object, Name)))
    {
        void* V = P->ContainerPtrToValuePtr<void>(Object);
        if (P->IsInteger()) P->SetIntPropertyValue(V, int64(Value));
        else P->SetFloatingPointPropertyValue(V, Value);
    }
}
UObject* ObjectValue(UObject* Object, const TCHAR* Name)
{
    auto* P = CastField<FObjectPropertyBase>(Property(Object, Name));
    return P ? P->GetObjectPropertyValue_InContainer(Object) : nullptr;
}
void Call(UObject* Object, const TCHAR* Name)
{
    if (Object) if (UFunction* F = Object->FindFunction(Name))
    {
        FStructOnScope Params(F);
        Object->ProcessEvent(F, Params.GetStructMemory());
    }
}
void Bind(UObject* Source, const TCHAR* Name, UObject* Target, FName Function, bool bRemove = false)
{
    if (auto* P = CastField<FMulticastDelegateProperty>(Property(Source, Name)))
    {
        FScriptDelegate Delegate;
        Delegate.BindUFunction(Target, Function);
        P->RemoveDelegate(Delegate, Source);
        if (!bRemove) P->AddDelegate(Delegate, Source);
    }
}
}

UGnorcThiefBehaviorComponent::UGnorcThiefBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Apply model orientation after the inherited floor-rotation graph and movement.
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UGnorcThiefBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Mesh = Character->GetMesh();
    MainMesh = Mesh ? Mesh->SkeletalMesh : nullptr;
    TArray<UActorComponent*> Components;
    Character->GetComponents(Components);
    for (UActorComponent* C : Components)
    {
        const FString Name = C->GetClass()->GetName();
        if (Name == TEXT("Damageable_Com_C")) Damageable = C;
        if (Name == TEXT("Walking_AI_Character_C")) WalkingAI = C;
        if (Name == TEXT("Drops_Items_C")) Dropper = C;
    }
    if (!Damageable || !WalkingAI || !Dropper || !Mesh || !MainMesh || !FinalMesh ||
        !IdleAnimation || !AlertAnimation || !RunAnimation || !HitAnimation || !FinalAnimation)
    {
        UE_LOG(LogTemp, Error, TEXT("Gnorc Thief %s is missing a required component or animation."), *GetOwner()->GetName());
        SetComponentTickEnabled(false);
        return;
    }
    AddTickPrerequisiteActor(Character);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    BindContracts();
    GnorcThief::SetNumber(Damageable, TEXT("Hit Points"), RemainingHits);
    GnorcThief::SetNumber(Character, TEXT("Corpse Poof Delay"), FinalAnimation->GetPlayLength() + 0.3f);
    GnorcThief::SetNumber(WalkingAI, TEXT("Death Launch Upwards Force"), 0);
    GnorcThief::SetNumber(WalkingAI, TEXT("Death Launch Forwards Force"), 0);
    EnterState(EGnorcThiefState::Idle);
}

void UGnorcThiefBehaviorComponent::BindContracts()
{
    // Replace only this actor's one-hit adapter, not the shared Blueprint or other listeners.
    GnorcThief::Bind(Damageable, TEXT("Call Deal_Damage"), WalkingAI, TEXT("On Damaged"), true);
    GnorcThief::Bind(Damageable, TEXT("Call Deal_Damage"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnAcceptedDamage));
    GnorcThief::Bind(Dropper, TEXT("Item Dropper Successfully Reset"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnDropperReset));
}

void UGnorcThiefBehaviorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GnorcThief::Bind(Damageable, TEXT("Call Deal_Damage"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnAcceptedDamage), true);
    GnorcThief::Bind(Dropper, TEXT("Item Dropper Successfully Reset"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnDropperReset), true);
    Super::EndPlay(Reason);
}

void UGnorcThiefBehaviorComponent::OnDropperReset()
{
    // The legacy reset removes entries while iterating its array and can leave survivors.
    // Finish resetting those gems through their normal contract before releasing our indices.
    if (auto* P = CastField<FArrayProperty>(GnorcThief::Property(Dropper, TEXT("Items_I_Dropped"))))
    {
        if (auto* Inner = CastField<FObjectPropertyBase>(P->Inner))
        {
            FScriptArrayHelper Array(P, P->ContainerPtrToValuePtr<void>(Dropper));
            TArray<TWeakObjectPtr<UObject>> Remaining;
            for (int32 I = 0; I < Array.Num(); ++I) Remaining.Add(Inner->GetObjectPropertyValue(Array.GetRawPtr(I)));
            for (const auto& Gem : Remaining) if (Gem.IsValid()) GnorcThief::Call(Gem.Get(), TEXT("Spawned Gem Reset"));
            Array.EmptyValues();
        }
    }
    RemainingHits = 3;
    ReleasedGemIndices.Reset();
    GemsSpawned = 0;
    GnorcThief::SetNumber(Damageable, TEXT("Hit Points"), RemainingHits);
    if (Mesh && MainMesh) Mesh->SetSkeletalMesh(MainMesh);
    EnterState(EGnorcThiefState::Idle);
}

void UGnorcThiefBehaviorComponent::SetMovementHeld(bool bHeld)
{
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement) return;
    if (bHeld)
    {
        if (!bHoldingMovement) PreviousMovementMode = uint8(Movement->MovementMode);
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        bHoldingMovement = true;
    }
    else if (bHoldingMovement)
    {
        Movement->SetMovementMode(EMovementMode(PreviousMovementMode));
        bHoldingMovement = false;
    }
}

void UGnorcThiefBehaviorComponent::FaceDirection(const FVector& Direction)
{
    if (Mesh && Direction.SizeSquared2D() > 1.f)
    {
        FRotator Rotation = Mesh->GetComponentRotation();
        Rotation.Yaw = Direction.Rotation().Yaw + MeshForwardYaw;
        Mesh->SetWorldRotation(Rotation);
    }
}

void UGnorcThiefBehaviorComponent::EnterState(EGnorcThiefState NewState)
{
    State = NewState;
    StateElapsed = 0.f;
    if (Mesh) LockedMeshYaw = Mesh->GetComponentRotation().Yaw;
    UAnimSequence* Animation = nullptr;
    bool bLoop = false;
    switch (State)
    {
    case EGnorcThiefState::Idle: Animation = IdleAnimation; bLoop = true; break;
    case EGnorcThiefState::Alert: Animation = AlertAnimation; break;
    case EGnorcThiefState::Flee: Animation = RunAnimation; bLoop = true; break;
    case EGnorcThiefState::HitRoll: Animation = HitAnimation; break;
    case EGnorcThiefState::FinalRoll:
        if (Mesh && FinalMesh) Mesh->SetSkeletalMesh(FinalMesh);
        Animation = FinalAnimation;
        break;
    case EGnorcThiefState::Dead: break;
    }
    SetMovementHeld(State == EGnorcThiefState::Alert || State == EGnorcThiefState::HitRoll || State == EGnorcThiefState::FinalRoll || State == EGnorcThiefState::Dead);
    if (Animation && Mesh && Mesh->SkeletalMesh && Animation->GetSkeleton() == Mesh->SkeletalMesh->GetSkeleton())
        Mesh->PlayAnimation(Animation, bLoop);
}

void UGnorcThiefBehaviorComponent::OnAcceptedDamage()
{
    if (!GetOwner()->HasAuthority() || RemainingHits <= 0 || State == EGnorcThiefState::HitRoll ||
        State == EGnorcThiefState::FinalRoll || State == EGnorcThiefState::Dead ||
        GnorcThief::Bool(Damageable, TEXT("Invincible")) || GnorcThief::Bool(Damageable, TEXT("Frozen")) ||
        GnorcThief::Bool(Dropper, TEXT("Reset_in_Progress"))) return;
    // The caller has already passed Damageable_Com's resistance and save-readiness checks.
    // Freeze/fear remain special effects and must not consume a thief hit.
    const int32 Type = GnorcThief::Number(Damageable, TEXT("Deal Damage - Damage Type"));
    if (Type == 7 || Type == 8) return;
    --RemainingHits;
    GnorcThief::SetNumber(Damageable, TEXT("Hit Points"), RemainingHits);
    if (AActor* Attacker = Cast<AActor>(GnorcThief::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage"))))
        FaceDirection(Character->GetActorLocation() - Attacker->GetActorLocation());
    const float ReactionYaw = Mesh->GetComponentRotation().Yaw;
    DropGemRange(2 - RemainingHits, RemainingHits == 0 ? 3 : 1);
    if (RemainingHits > 0)
    {
        EnterState(EGnorcThiefState::HitRoll);
        return;
    }
    // The inherited final-death listeners still own collision, poof and persistence lifecycle.
    // Suppress their all-at-once payout; the indexed 1/1/3 drops above already own it.
    const bool bPreviouslySuppressed = GnorcThief::Bool(Dropper, TEXT("Cannot_Drop_Items"));
    GnorcThief::SetBool(Dropper, TEXT("Cannot_Drop_Items"), true);
    if (auto* P = CastField<FMulticastDelegateProperty>(GnorcThief::Property(Damageable, TEXT("Damage Was Successfully Dealt"))))
        if (const auto* Delegate = P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable)))
            Delegate->ProcessMulticastDelegate<UObject>(nullptr);
    GnorcThief::SetBool(Dropper, TEXT("Cannot_Drop_Items"), bPreviouslySuppressed);
    // Base_AI_Character turns the actor toward its attacker during death. Keep the roll facing away.
    Mesh->SetWorldRotation(FRotator(0.f, ReactionYaw, 0.f));
    EnterState(EGnorcThiefState::FinalRoll);
}

void UGnorcThiefBehaviorComponent::DropGemRange(int32 First, int32 Count)
{
    if (GnorcThief::Bool(Dropper, TEXT("Cannot_Drop_Items"))) return;
    GnorcThief::Call(Dropper, TEXT("Remove Gems We Perma Collected"));
    GnorcThief::Call(Dropper, TEXT("Find All Items I Have But Shouldn't Drop"));
    auto* ItemsProperty = CastField<FArrayProperty>(GnorcThief::Property(Dropper, TEXT("Items_to_Drop")));
    auto* PendingProperty = CastField<FArrayProperty>(GnorcThief::Property(Dropper, TEXT("Items_I_Have_But_Shouldnt_Drop")));
    auto* SpawnedProperty = CastField<FArrayProperty>(GnorcThief::Property(Dropper, TEXT("Items_I_Dropped")));
    if (!ItemsProperty || !PendingProperty || !SpawnedProperty) return;
    auto* ItemClass = CastField<FObjectPropertyBase>(ItemsProperty->Inner);
    auto* PendingBool = CastField<FBoolProperty>(PendingProperty->Inner);
    auto* SpawnedObject = CastField<FObjectPropertyBase>(SpawnedProperty->Inner);
    if (!ItemClass || !PendingBool || !SpawnedObject) return;
    FScriptArrayHelper Items(ItemsProperty, ItemsProperty->ContainerPtrToValuePtr<void>(Dropper));
    FScriptArrayHelper Pending(PendingProperty, PendingProperty->ContainerPtrToValuePtr<void>(Dropper));
    for (int32 Index = First; Index < First + Count; ++Index)
    {
        if (!Items.IsValidIndex(Index) || ReleasedGemIndices.Contains(Index)) continue;
        if (Pending.IsValidIndex(Index) && PendingBool->GetPropertyValue(Pending.GetRawPtr(Index))) continue;
        UClass* GemClass = Cast<UClass>(ItemClass->GetObjectPropertyValue(Items.GetRawPtr(Index)));
        if (!GemClass) continue; // Permanently collected entries retain their indices and become null.
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, GnorcThief::Number(Dropper, TEXT("Item_Spawn_Height_Offset")));
        AActor* Gem = GetWorld()->SpawnActor<AActor>(GemClass, Location, GetOwner()->GetActorRotation(), Spawn);
        if (!Gem) continue;
        UFunction* Initialize = Gem->FindFunction(TEXT("Gem_Spawn_Process"));
        if (!Initialize) { Gem->Destroy(); continue; }
        FStructOnScope Params(Initialize);
        for (TFieldIterator<FProperty> It(Initialize); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            const FString Name = GnorcThief::Key(It->GetName());
            void* Value = It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
            if (auto* P = CastField<FObjectPropertyBase>(*It))
            {
                if (Name == TEXT("playerwhospawnedme")) P->SetObjectPropertyValue(Value, GnorcThief::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
                if (Name == TEXT("objectispawnedfrom")) P->SetObjectPropertyValue(Value, GetOwner());
            }
            else if (auto* StringParam = CastField<FStrProperty>(*It))
            {
                if (Name == TEXT("nameofobjectwhospawnedme")) StringParam->SetPropertyValue(Value, GetOwner()->GetName());
            }
            else if (auto* IndexParam = CastField<FIntProperty>(*It))
            {
                if (Name == TEXT("spawnindex")) IndexParam->SetPropertyValue(Value, Index);
            }
        }
        // Register with the original dropper so checkpoint resets clean up emitted gems.
        FScriptArrayHelper Spawned(SpawnedProperty, SpawnedProperty->ContainerPtrToValuePtr<void>(Dropper));
        SpawnedObject->SetObjectPropertyValue(Spawned.GetRawPtr(Spawned.AddValue()), Gem);
        ReleasedGemIndices.Add(Index);
        ++GemsSpawned;
        if (UStaticMeshComponent* GemMesh = Gem->FindComponentByClass<UStaticMeshComponent>())
            if (GemMesh->IsSimulatingPhysics()) GemMesh->AddImpulse(FVector((Index % 3 - 1) * 100.f, 0, 550), NAME_None, true);
        Gem->ProcessEvent(Initialize, Params.GetStructMemory());
    }
}

void UGnorcThiefBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!Character || !WalkingAI || !Mesh) return;
    if (bFirstTick) { BindContracts(); bFirstTick = false; }
    StateElapsed += FMath::Max(DeltaTime, 0.f);
    if (GnorcThief::Bool(Damageable, TEXT("Frozen")) || GnorcThief::Bool(Damageable, TEXT("Paralyzed_by_Fear"))) return;
    const int32 AIState = GnorcThief::Number(WalkingAI, TEXT("AICharacter_State"));
    if (RemainingHits > 0 && AIState == 3) { EnterState(EGnorcThiefState::Dead); return; }
    switch (State)
    {
    case EGnorcThiefState::Idle:
        if (AIState == 1 || AIState == 2) EnterState(EGnorcThiefState::Alert);
        break;
    case EGnorcThiefState::Alert:
        SetMovementHeld(true);
        Mesh->SetWorldRotation(FRotator(0.f, LockedMeshYaw, 0.f));
        if (StateElapsed >= AlertAnimation->GetPlayLength()) EnterState(AIState == 0 ? EGnorcThiefState::Idle : EGnorcThiefState::Flee);
        break;
    case EGnorcThiefState::Flee:
        FaceDirection(Character->GetVelocity());
        if (AIState == 0) EnterState(EGnorcThiefState::Idle);
        break;
    case EGnorcThiefState::HitRoll:
        SetMovementHeld(true);
        Mesh->SetWorldRotation(FRotator(0.f, LockedMeshYaw, 0.f));
        if (StateElapsed >= HitAnimation->GetPlayLength() + RecoverySeconds) EnterState(EGnorcThiefState::Flee);
        break;
    case EGnorcThiefState::FinalRoll:
        SetMovementHeld(true);
        Mesh->SetWorldRotation(FRotator(0.f, LockedMeshYaw, 0.f));
        if (StateElapsed >= FinalAnimation->GetPlayLength() + RecoverySeconds)
        {
            EnterState(EGnorcThiefState::Dead);
            GnorcThief::Call(WalkingAI, TEXT("Disable Collission Due to Death"));
            if (!Character->IsHidden()) GnorcThief::Call(WalkingAI, TEXT("Poof Away Corpse"));
        }
        break;
    case EGnorcThiefState::Dead: break;
    }
}
