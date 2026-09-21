#include "GnorcThiefBehaviorComponent.h"
#include "GnorcThiefAnimInstance.h"
#include "GameFramework/Controller.h"

#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
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

namespace
{
constexpr float OriginalStep = 1.f / 30.f;
const int32 FrameCounts[] = {34, 14, 10, 10, 20, 20};
float OctDistance2D(const FVector& Delta)
{
    const float X = FMath::Abs(Delta.X), Y = FMath::Abs(Delta.Y);
    return FMath::Max(X, Y) + 0.375f * FMath::Min(X, Y);
}
float Angle(const FVector& Delta)
{
    // The original heading is one byte, including the target produced by Atan2.
    return FMath::RoundToFloat(Delta.Rotation().Yaw * 256.f / 360.f) * 360.f / 256.f;
}
}

UGnorcThiefBehaviorComponent::UGnorcThiefBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    RoutePoints = {FVector(0,0,0), FVector(-6083,-4096,-123), FVector(6144,-8479,-472),
        FVector(8192,-2048,-267), FVector(18268,-6134,-359), FVector(16179,819,-318),
        FVector(8397,6175,829), FVector(809,8192,-461), FVector(-8192,6144,-441)};
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
        if (C->GetFName() == TEXT("ThiefBodyCollision")) BodyCollision = Cast<UBoxComponent>(C);
        if (C->GetFName() == TEXT("ThiefChargeSensor")) ChargeSensor = Cast<UBoxComponent>(C);
    }
    if (!Damageable || !WalkingAI || !Dropper || !MainMesh || !FinalMesh || !IdleAnimation ||
        !AlertAnimation || !RunAnimation || !AlternateRunAnimation || !HitAnimation || !FinalAnimation ||
        RoutePoints.Num() < 2 || WorldUnitsPerOriginalUnit <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("Gnorc Thief %s has incomplete fidelity configuration."), *GetOwner()->GetName());
        SetComponentTickEnabled(false); return;
    }
    RouteOrigin = FTransform(FRotator(0, Character->GetActorRotation().Yaw, 0), Character->GetActorLocation());
    HeadingDegrees = Character->GetActorRotation().Yaw;
    Mesh->AddTickPrerequisiteComponent(this);
    BindContracts();
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &UGnorcThiefBehaviorComponent::OnChargeSensorOverlap);
    GnorcThief::SetNumber(Damageable, TEXT("Hit Points"), RemainingHits);
    GnorcThief::SetNumber(Character, TEXT("Corpse Poof Delay"), 10.f);
    GnorcThief::SetNumber(WalkingAI, TEXT("Death Launch Upwards Force"), 0);
    GnorcThief::SetNumber(WalkingAI, TEXT("Death Launch Forwards Force"), 0);
    TakeMovementControl();
    SelectClip(0, false);
    EnterState(EGnorcThiefState::Idle);
}

void UGnorcThiefBehaviorComponent::TakeMovementControl()
{
    // Component ticking is independent of actor ticking. Shared damage/reset delegates stay bound.
    Character->SetActorTickEnabled(false);
    WalkingAI->SetComponentTickEnabled(false);
    if (AController* Controller = Character->GetController()) Controller->StopMovement();
    auto* Movement = Character->GetCharacterMovement();
    Movement->DisableMovement();
    Movement->SetComponentTickEnabled(false);
    Movement->bEnablePhysicsInteraction = false;
    Character->ConsumeMovementInputVector();
    if (Mesh->GetAnimClass() != UGnorcThiefAnimInstance::StaticClass())
        Mesh->SetAnimInstanceClass(UGnorcThiefAnimInstance::StaticClass());
}

UAnimSequence* UGnorcThiefBehaviorComponent::AnimationForClip(int32 Clip) const
{
    switch (Clip) { case 0: return IdleAnimation; case 1: return AlertAnimation; case 2: return RunAnimation;
        case 3: return AlternateRunAnimation; case 4: return HitAnimation; case 5: return FinalAnimation; default: return nullptr; }
}
void UGnorcThiefBehaviorComponent::GetPoseInputs(UAnimSequence*& A, UAnimSequence*& B, float& TimeA, float& TimeB, float& Alpha) const
{
    A = AnimationForClip(CurrentClip); B = AnimationForClip(NextClip);
    TimeA = A ? A->GetPlayLength() * CurrentFrame / (FrameCounts[CurrentClip] - 1) : 0.f;
    TimeB = B ? B->GetPlayLength() * NextFrame / (FrameCounts[NextClip] - 1) : 0.f;
    Alpha = FMath::Clamp(Progress / 64.f, 0.f, 1.f);
}
void UGnorcThiefBehaviorComponent::SelectClip(int32 Clip, bool bBlend)
{
    if (bBlend && NextClip == Clip) return;
    if (bBlend)
    {
        CurrentClip = NextClip; CurrentFrame = NextFrame;
        NextClip = Clip; NextFrame = 0; Progress = ProgressPerStep = 16;
    }
    else
    {
        CurrentClip = NextClip = Clip; CurrentFrame = 0; NextFrame = 1;
        Progress = 0; ProgressPerStep = 32;
    }
}
bool UGnorcThiefBehaviorComponent::AdvanceAnimation()
{
    Progress += ProgressPerStep;
    if (Progress < 64) return false;
    Progress &= 63;
    bool bComplete = false;
    if (CurrentClip != NextClip)
    {
        CurrentClip = NextClip; CurrentFrame = NextFrame; ++NextFrame;
        Progress = 0; ProgressPerStep = 32;
    }
    else
    {
        CurrentFrame = NextFrame; ++NextFrame;
        if (NextFrame >= FrameCounts[CurrentClip]) { NextFrame = 0; bComplete = true; }
    }
    EmitFrameSound();
    return bComplete;
}
void UGnorcThiefBehaviorComponent::EmitFrameSound()
{
    int32 Slot = INDEX_NONE;
    if (CurrentClip == 1 && CurrentFrame == 0) Slot = 0;
    if (CurrentClip == 2 || CurrentClip == 3)
    { if (CurrentFrame == 1) Slot = 1; if (CurrentFrame == 6) Slot = 2; }
    if (CurrentClip == 4)
    { if (CurrentFrame == 0) Slot = 3; if (CurrentFrame == 8) Slot = 4; if (CurrentFrame == 13) Slot = 5; }
    if (CurrentClip == 5)
    { if (CurrentFrame == 1) Slot = 3; if (CurrentFrame == 8) Slot = 6; if (CurrentFrame == 14) Slot = 7; }
    if (Slot == INDEX_NONE) return;
    // Bounded diagnostic log; do not grow indefinitely on a running level.
    if (SoundCueHistory.Num() >= 256) SoundCueHistory.RemoveAt(0);
    SoundCueHistory.Add(Slot);
    if (!OriginalSounds.IsValidIndex(Slot) || !OriginalSounds[Slot] || GetNetMode() == NM_DedicatedServer) return;
    PlayingSounds.RemoveAll([](UAudioComponent* Audio) { return !IsValid(Audio) || !Audio->IsPlaying(); });
    USoundAttenuation* Attenuation = Slot == 0 && AlertSoundAttenuation ? AlertSoundAttenuation : SoundAttenuation;
    VoiceAudio = UGameplayStatics::SpawnSoundAttached(OriginalSounds[Slot], Character->GetRootComponent(), NAME_None,
        FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
        FMath::Max(0.f, SoundVolume) * (Slot == 0 ? FMath::Max(0.f, AlertVolumeMultiplier) : 1.f), 1.f, 0.f, Attenuation);
    if (VoiceAudio) PlayingSounds.Add(VoiceAudio);
}
void UGnorcThiefBehaviorComponent::StopSounds()
{
    for (UAudioComponent* Audio : PlayingSounds) if (IsValid(Audio)) Audio->Stop();
    PlayingSounds.Reset(); VoiceAudio = nullptr;
}
void UGnorcThiefBehaviorComponent::EnterState(EGnorcThiefState NewState)
{
    State = NewState;
    if (BodyCollision) BodyCollision->SetCollisionEnabled(State == EGnorcThiefState::Dead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
    if (ChargeSensor) ChargeSensor->SetCollisionEnabled(State == EGnorcThiefState::Dead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
    switch (State)
    {
    case EGnorcThiefState::Idle: SelectClip(0, true); break;
    case EGnorcThiefState::Alert: SelectClip(1, true); break;
    case EGnorcThiefState::Flee: RunPhase = 2; SelectClip(2, true); break;
    case EGnorcThiefState::HitRoll: SelectClip(4, true); break;
    case EGnorcThiefState::FinalRoll:
        // InitAnim can evaluate immediately: publish the matching clip before changing skeletons.
        SelectClip(5, false);
        Mesh->SetSkeletalMesh(FinalMesh);
        Mesh->SetAnimInstanceClass(UGnorcThiefAnimInstance::StaticClass());
        SlideDisplacement = 280.f; break;
    case EGnorcThiefState::Dead: break;
    }
}
FVector UGnorcThiefBehaviorComponent::FloorPosition(AActor* Actor) const
{
    FVector Position = Actor->GetActorLocation();
    if (const ACharacter* C = Cast<ACharacter>(Actor)) Position.Z -= C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return Position;
}
FVector UGnorcThiefBehaviorComponent::RoutePosition(int32 Index) const
{
    return RouteOrigin.TransformPosition(RoutePoints[Index] * WorldUnitsPerOriginalUnit);
}
float UGnorcThiefBehaviorComponent::OriginalDistanceTo(const FVector& Position) const
{
    return OctDistance2D(Position - Character->GetActorLocation()) / WorldUnitsPerOriginalUnit;
}
void UGnorcThiefBehaviorComponent::FaceSpyro()
{
    if (!IsValid(Pursuer)) return;
    HeadingDegrees = FMath::UnwindDegrees(HeadingDegrees + FMath::Clamp(
        FMath::FindDeltaAngleDegrees(HeadingDegrees, Angle(Pursuer->GetActorLocation() - Character->GetActorLocation())), -11.25f, 11.25f));
}
bool UGnorcThiefBehaviorComponent::FollowRoute()
{
    CurrentRouteNode = FMath::Clamp(CurrentRouteNode, 0, RoutePoints.Num() - 1);
    if (OriginalDistanceTo(RoutePosition(CurrentRouteNode)) < 1024.f)
    {
        if (!IsValid(Pursuer) || OriginalDistanceTo(Pursuer->GetActorLocation()) > 5120.f ||
            FMath::Abs(FloorPosition(Pursuer).Z - FloorPosition(Character).Z) > 2048.f * WorldUnitsPerOriginalUnit)
        { EnterState(EGnorcThiefState::Idle); return false; }
        const int32 Forward = (CurrentRouteNode + 1) % RoutePoints.Num();
        const int32 Backward = (CurrentRouteNode + RoutePoints.Num() - 1) % RoutePoints.Num();
        const FVector Location = Character->GetActorLocation();
        const float PlayerAngle = Angle(Pursuer->GetActorLocation() - Location);
        const float ForwardSeparation = FMath::Abs(FMath::FindDeltaAngleDegrees(PlayerAngle, Angle(RoutePosition(Forward) - Location)));
        const float BackwardSeparation = FMath::Abs(FMath::FindDeltaAngleDegrees(PlayerAngle, Angle(RoutePosition(Backward) - Location)));
        CurrentRouteNode = BackwardSeparation < ForwardSeparation ? Forward : Backward;
    }
    const float Error = FMath::FindDeltaAngleDegrees(HeadingDegrees, Angle(RoutePosition(CurrentRouteNode) - Character->GetActorLocation()));
    HeadingDegrees = FMath::UnwindDegrees(HeadingDegrees + FMath::Clamp(Error, -11.25f, 11.25f));
    if (FMath::Abs(Error) < 45.f) GroundMove(State == EGnorcThiefState::HitRoll ? 210.f : 96.f, HeadingDegrees);
    return true;
}
void UGnorcThiefBehaviorComponent::GroundMove(float OriginalDistance, float Direction)
{
    const FVector Before = Character->GetActorLocation();
    FVector Delta = FRotator(0, Direction, 0).Vector() * OriginalDistance * WorldUnitsPerOriginalUnit;
    // Sweep the solid body's footprint against players as well: attached boxes alone are not swept by CharacterMovement.
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GnorcThiefMovement), false, Character);
    if (BodyCollision && !Delta.IsNearlyZero())
    {
        FCollisionObjectQueryParams Pawns;
        Pawns.AddObjectTypesToQuery(ECC_Pawn); Pawns.AddObjectTypesToQuery(ECC_GameTraceChannel4);
        FHitResult Contact;
        if (GetWorld()->SweepSingleByObjectType(Contact, Before, Before + Delta, FQuat::Identity, Pawns,
            FCollisionShape::MakeBox(BodyCollision->GetScaledBoxExtent()), Query))
            Delta *= FMath::Max(0.f, Contact.Time - 0.01f);
    }
    FCollisionObjectQueryParams GroundTypes;
    GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic); GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Probe = Before + Delta - FVector(0,0,HalfHeight);
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByObjectType(Floor, Probe + FVector(0,0,1024.f * WorldUnitsPerOriginalUnit),
        Probe - FVector(0,0,5000.f * WorldUnitsPerOriginalUnit), GroundTypes, Query) || Floor.ImpactNormal.Z < 0.5f) return;
    const float Difference = Floor.ImpactPoint.Z + HalfHeight + 2.f - Before.Z;
    Delta.Z = FMath::Abs(Difference) > 600.f * WorldUnitsPerOriginalUnit ?
        FMath::Sign(Difference) * 250.f * WorldUnitsPerOriginalUnit : Difference;
    auto* Movement = Character->GetCharacterMovement();
    FHitResult Hit;
    Movement->SafeMoveUpdatedComponent(Delta, Character->GetActorQuat(), true, Hit);
    if (Hit.IsValidBlockingHit()) static_cast<UMovementComponent*>(Movement)->SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
    LastStepDelta = Character->GetActorLocation() - Before;
    Movement->Velocity = LastStepDelta / OriginalStep;
}
void UGnorcThiefBehaviorComponent::OnAcceptedDamage()
{
    if (!GetOwner()->HasAuthority() || RemainingHits <= 0 || State == EGnorcThiefState::HitRoll ||
        State == EGnorcThiefState::FinalRoll || State == EGnorcThiefState::Dead ||
        GnorcThief::Bool(Damageable, TEXT("Invincible")) || GnorcThief::Bool(Damageable, TEXT("Frozen")) ||
        GnorcThief::Bool(Dropper, TEXT("Reset_in_Progress"))) return;
    const int32 Type = GnorcThief::Number(Damageable, TEXT("Deal Damage - Damage Type"));
    if (Type == 7 || Type == 8) return;
    --RemainingHits;
    GnorcThief::SetNumber(Damageable, TEXT("Hit Points"), RemainingHits);
    AActor* Attacker = Cast<AActor>(GnorcThief::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
    if (Attacker) Pursuer = Attacker;
    DropGemRange(2 - RemainingHits, RemainingHits == 0 ? 3 : 1);
    if (RemainingHits > 0) { EnterState(EGnorcThiefState::HitRoll); return; }
    FinalSlideHeading = HeadingDegrees;
    if (Attacker)
    {
        const float Away = Angle(Character->GetActorLocation() - Attacker->GetActorLocation());
        const float PlayerHeading = Attacker->GetActorRotation().Yaw;
        const float Limited = PlayerHeading + FMath::Clamp(FMath::FindDeltaAngleDegrees(PlayerHeading, Away), -45.f, 45.f);
        FinalSlideHeading = PlayerHeading + 0.25f * FMath::FindDeltaAngleDegrees(PlayerHeading, Limited);
    }
    const bool Suppressed = GnorcThief::Bool(Dropper, TEXT("Cannot_Drop_Items"));
    GnorcThief::SetBool(Dropper, TEXT("Cannot_Drop_Items"), true);
    auto* Scream = CastField<FObjectPropertyBase>(GnorcThief::Property(WalkingAI, TEXT("Death_Scream")));
    UObject* Previous = Scream ? Scream->GetObjectPropertyValue_InContainer(WalkingAI) : nullptr;
    if (Scream) Scream->SetObjectPropertyValue_InContainer(WalkingAI, nullptr);
    if (auto* P = CastField<FMulticastDelegateProperty>(GnorcThief::Property(Damageable, TEXT("Damage Was Successfully Dealt"))))
        if (const auto* Delegate = P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable)))
            Delegate->ProcessMulticastDelegate<UObject>(nullptr);
    GnorcThief::SetBool(Dropper, TEXT("Cannot_Drop_Items"), Suppressed);
    if (Scream) Scream->SetObjectPropertyValue_InContainer(WalkingAI, Previous);
    TakeMovementControl();
    EnterState(EGnorcThiefState::FinalRoll);
}
void UGnorcThiefBehaviorComponent::StepOriginal()
{
    ++SimulationTicks;
    LastStepDelta = FVector::ZeroVector;
    Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    if (State == EGnorcThiefState::Dead) return;
    const bool Complete = AdvanceAnimation();
    switch (State)
    {
    case EGnorcThiefState::Idle:
        GroundMove(0, HeadingDegrees);
        if (IsValid(Pursuer))
        {
            const float Distance = OriginalDistanceTo(Pursuer->GetActorLocation());
            if (Distance < 5120.f && FMath::Abs(FloorPosition(Pursuer).Z - FloorPosition(Character).Z) < 1024.f * WorldUnitsPerOriginalUnit)
                EnterState(EGnorcThiefState::Alert);
            else if (Distance < 10240.f) FaceSpyro();
        }
        break;
    case EGnorcThiefState::Alert:
        FaceSpyro();
        if (Complete) EnterState(EGnorcThiefState::Flee);
        break;
    case EGnorcThiefState::Flee:
    case EGnorcThiefState::HitRoll:
        if (FollowRoute() && Complete)
        {
            if (State == EGnorcThiefState::Flee && RunPhase == 2) { RunPhase = 3; SelectClip(3, false); }
            else EnterState(EGnorcThiefState::Flee);
        }
        break;
    case EGnorcThiefState::FinalRoll:
        if (Complete)
        {
            EnterState(EGnorcThiefState::Dead);
            GnorcThief::Call(WalkingAI, TEXT("Disable Collission Due to Death"));
            if (!Character->IsHidden()) GnorcThief::Call(WalkingAI, TEXT("Poof Away Corpse"));
        }
        else if (SlideDisplacement > 0.f)
        { GroundMove(SlideDisplacement, FinalSlideHeading); SlideDisplacement = FMath::Max(0.f, SlideDisplacement - 12.f); }
        break;
    case EGnorcThiefState::Dead: break;
    }
    Mesh->SetWorldRotation(FRotator(0, HeadingDegrees + MeshForwardYaw, 0));
}
void UGnorcThiefBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!Character || !WalkingAI || !Mesh || !GetOwner()->HasAuthority()) return;
    if (bFirstTick) { BindContracts(); bFirstTick = false; }
    if (!IsValid(Pursuer)) Pursuer = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    TakeMovementControl();
    if (GnorcThief::Bool(Damageable, TEXT("Frozen")) || GnorcThief::Bool(Damageable, TEXT("Paralyzed_by_Fear")) ||
        GnorcThief::Bool(Dropper, TEXT("Reset_in_Progress"))) return;
    Accumulator += FMath::Max(DeltaTime, 0.f);
    // Bounded catch-up prevents a long stall from teleporting the enemy across the level.
    int32 Steps = 0;
    while (Accumulator + KINDA_SMALL_NUMBER >= OriginalStep && Steps++ < 30)
    { Accumulator = FMath::Max(0.f, Accumulator - OriginalStep); StepOriginal(); }
    Accumulator = FMath::Min(Accumulator, OriginalStep);
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
    StopSounds();
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.RemoveDynamic(this, &UGnorcThiefBehaviorComponent::OnChargeSensorOverlap);
    GnorcThief::Bind(Damageable, TEXT("Call Deal_Damage"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnAcceptedDamage), true);
    GnorcThief::Bind(Dropper, TEXT("Item Dropper Successfully Reset"), this, GET_FUNCTION_NAME_CHECKED(UGnorcThiefBehaviorComponent, OnDropperReset), true);
    Super::EndPlay(Reason);
}

void UGnorcThiefBehaviorComponent::OnChargeSensorOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!Character || !OtherActor || OtherActor == Character || !OtherComponent ||
        OtherComponent->GetCollisionObjectType() != ECC_GameTraceChannel4 || RemainingHits <= 0 ||
        (State != EGnorcThiefState::Idle && State != EGnorcThiefState::Alert && State != EGnorcThiefState::Flee)) return;
    // The solid body stops a charging player before the original capsule overlaps.
    // Deliver the same capsule event to the existing charge/resistance/damage contract.
    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    Capsule->OnComponentBeginOverlap.Broadcast(Capsule, OtherActor, OtherComponent, OtherBodyIndex, bFromSweep, SweepResult);
}


void UGnorcThiefBehaviorComponent::OnDropperReset()
{
    StopSounds();
    SoundCueHistory.Reset();
    CurrentRouteNode = 0;
    Accumulator = 0.f;
    SlideDisplacement = 0.f;
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
    SelectClip(0, false);
    if (Mesh && MainMesh) Mesh->SetSkeletalMesh(MainMesh);
    TakeMovementControl();
    EnterState(EGnorcThiefState::Idle);
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
