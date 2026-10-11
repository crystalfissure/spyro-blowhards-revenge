#include "SnowGnorcBehaviorComponent.h"
#include "SpyroMeleeContact.h"
#include "SnowGnorcAnimInstance.h"
#include "ToastyEncounterCollision.h"
#include "GameFramework/Controller.h"

#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/ConstructorHelpers.h"

namespace SnowGnorc
{
void IgnorePlayer(FCollisionQueryParams& Query, AActor* Player, bool IncludePlayer = true)
{
    if (!IsValid(Player)) return;
    if (IncludePlayer) Query.AddIgnoredActor(Player);
    // Sparx is a child actor with a blocking capsule, not a component of Spyro.
    TArray<AActor*> Children;
    Player->GetAllChildActors(Children, true);
    Query.AddIgnoredActors(Children);
}
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
constexpr float GnorcStep=1.f/30.f;
const int32 GnorcFrames[]={25,1,4,7,19,4,18,12,40,40};
const int32 GnorcRates[]={16,0,16,16,32,16,16,16,16,16};
float GnorcDistance(const FVector& V) { return FMath::Max(FMath::Abs(V.X),FMath::Abs(V.Y))+.375f*FMath::Min(FMath::Abs(V.X),FMath::Abs(V.Y)); }
}
USnowGnorcBehaviorComponent::USnowGnorcBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics;
    static ConstructorHelpers::FObjectFinder<UParticleSystem> Smoke(TEXT("/Game/SpyroContent/Global_Assets/Global_Particles/P_DustCloud.P_DustCloud"));
    DeathSmoke=Smoke.Object;
}
void USnowGnorcBehaviorComponent::BeginPlay()
{
    Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Mesh=Character->GetMesh();
    for (auto* C:Character->GetComponents())
    {
        if (C->GetClass()->GetName()==TEXT("Damageable_Com_C")) Damageable=C;
        if (C->GetClass()->GetName()==TEXT("Walking_AI_Character_C")) WalkingAI=C;
        if (C->GetClass()->GetName()==TEXT("Drops_Items_C")) Dropper=C;
        if (C->GetFName()==TEXT("SnowGnorcBodyCollision")) BodyCollision=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("SnowGnorcChargeSensor")) ChargeSensor=Cast<UBoxComponent>(C);
        if (C->GetFName()==TEXT("Attack_Radius")) if (auto* A=Cast<UPrimitiveComponent>(C))
        { A->SetGenerateOverlapEvents(false); A->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    }
    if (!Mesh || !Damageable || !WalkingAI || !Dropper || !BodyCollision || !ChargeSensor ||
        Animations.Num()!=10 || Animations.Contains(nullptr) || OriginalSounds.Num()!=11 || OriginalSounds.Contains(nullptr))
    { UE_LOG(LogTemp,Error,TEXT("Snow Gnorc %s has incomplete contracts/assets."),*Character->GetName()); SetComponentTickEnabled(false); return; }
    Home=Character->GetActorTransform(); Heading=PreviousHeading=Home.Rotator().Yaw;
    PreviousLocation=Home.GetLocation(); MeshOffset=Mesh->GetRelativeLocation();
    Mesh->AddTickPrerequisiteComponent(this);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->bEnableUpdateRateOptimizations=false;
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    BoneTransformsHandle=Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&USnowGnorcBehaviorComponent::UpdateBodyCollision));
    SnowGnorc::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
    SnowGnorc::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USnowGnorcBehaviorComponent,OnAcceptedDamage));
    SnowGnorc::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(USnowGnorcBehaviorComponent,OnDropperReset));
    ChargeSensor->OnComponentBeginOverlap.AddUniqueDynamic(this,&USnowGnorcBehaviorComponent::OnChargeSensorOverlap);
    SnowGnorc::SetNumber(Damageable,TEXT("Hit Points"),1);
    SnowGnorc::SetBool(WalkingAI,TEXT("Poofs_On_Death"),false);
    SnowGnorc::SetNumber(WalkingAI,TEXT("Death Launch Upwards Force"),0);
    SnowGnorc::SetNumber(WalkingAI,TEXT("Death Launch Forwards Force"),0);
    SnowGnorc::SetNumber(Character,TEXT("Corpse Poof Delay"),100000.f);
    TakeMovementControl(); SelectClip(0,false);
    ToastyEncounterCollision::LiftFromFloor(Character,Pursuer,WorldUnitsPerOriginalUnit);
    Home=Character->GetActorTransform(); PreviousLocation=Home.GetLocation();
}
void USnowGnorcBehaviorComponent::TakeMovementControl()
{
    Character->SetActorTickEnabled(false); WalkingAI->SetComponentTickEnabled(false);
    if (auto* C=Character->GetController()) C->StopMovement();
    auto* M=Character->GetCharacterMovement(); M->DisableMovement(); M->SetComponentTickEnabled(false); M->bEnablePhysicsInteraction=false;
    Character->ConsumeMovementInputVector();
    if (Mesh->GetAnimClass()!=USnowGnorcAnimInstance::StaticClass()) Mesh->SetAnimInstanceClass(USnowGnorcAnimInstance::StaticClass());
}

void USnowGnorcBehaviorComponent::SelectClip(int32 Clip,bool Blend)
{
    Clip=FMath::Clamp(Clip,0,9);
    if (Blend && NextClip==Clip) return;
    if (Blend) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextClip=Clip; NextFrame=0; Progress=ProgressPerStep=16; }
    else { CurrentClip=NextClip=Clip; CurrentFrame=Progress=0; NextFrame=GnorcFrames[Clip]>1?1:0; ProgressPerStep=GnorcRates[Clip]; EmitFrameSound(); }
}
bool USnowGnorcBehaviorComponent::AdvanceAnimation()
{
    Progress+=ProgressPerStep; if (Progress<64) return false; Progress&=63; bool Complete=false;
    if (CurrentClip!=NextClip) { CurrentClip=NextClip; CurrentFrame=NextFrame; NextFrame=GnorcFrames[CurrentClip]>1?1:0; Progress=0; ProgressPerStep=GnorcRates[CurrentClip]; }
    else { CurrentFrame=NextFrame; if (++NextFrame>=GnorcFrames[CurrentClip]) { NextFrame=0; Complete=true; } }
    EmitFrameSound(); return Complete;
}
void USnowGnorcBehaviorComponent::GetPoseInputs(UAnimSequence*& A,UAnimSequence*& B,float& TimeA,float& TimeB,float& Alpha) const
{
    A=Animations.IsValidIndex(CurrentClip)?Animations[CurrentClip]:nullptr; B=Animations.IsValidIndex(NextClip)?Animations[NextClip]:nullptr;
    TimeA=A?A->GetPlayLength()*CurrentFrame/FMath::Max(1,GnorcFrames[CurrentClip]-1):0;
    TimeB=B?B->GetPlayLength()*NextFrame/FMath::Max(1,GnorcFrames[NextClip]-1):0;
    Alpha=FMath::Clamp((Progress+Accumulator/GnorcStep*ProgressPerStep)/64.f,0.f,1.f);
}
void USnowGnorcBehaviorComponent::PlaySound(int32 Slot)
{
    if (SoundCueHistory.Num()>=256) SoundCueHistory.RemoveAt(0); SoundCueHistory.Add(Slot);
    PlayingSounds.RemoveAll([](UAudioComponent* A){return !IsValid(A)||!A->IsPlaying();});
    if (OriginalSounds.IsValidIndex(Slot) && OriginalSounds[Slot])
        if (auto* A=UGameplayStatics::SpawnSoundAttached(OriginalSounds[Slot],Character->GetRootComponent(),NAME_None,FVector::ZeroVector,EAttachLocation::KeepRelativeOffset,true,1,1,0,SoundAttenuation)) PlayingSounds.Add(A);
}

void USnowGnorcBehaviorComponent::EmitFrameSound()
{
    if (CurrentClip==4 && CurrentFrame==8) PlaySound(0);
    if (CurrentClip==6) { if (CurrentFrame==0) PlaySound(1); if (CurrentFrame==6) PlaySound(2); if (CurrentFrame==10) PlaySound(3); }
    if (CurrentClip==7) { if (CurrentFrame==2) PlaySound(4); if (CurrentFrame==5) PlaySound(5); if (CurrentFrame==7) PlaySound(6); }
    if (CurrentClip==8 || CurrentClip==9) { if (CurrentFrame==8) PlaySound(7); if (CurrentFrame==19) PlaySound(8); if (CurrentFrame==32) PlaySound(CurrentClip==8?9:10); }
}
void USnowGnorcBehaviorComponent::StopSounds() { for (auto* A:PlayingSounds) if (IsValid(A)) A->Stop(); PlayingSounds.Reset(); }
FVector USnowGnorcBehaviorComponent::Feet(AActor* Actor) const
{
    FVector P=Actor->GetActorLocation(); if (auto* C=Cast<ACharacter>(Actor)) P.Z-=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); return P;
}

float USnowGnorcBehaviorComponent::Distance() const { return IsValid(Pursuer)?GnorcDistance(Feet(Pursuer)-Feet(Character))/WorldUnitsPerOriginalUnit:MAX_flt; }
bool USnowGnorcBehaviorComponent::ComparableHeight() const { return IsValid(Pursuer) && FMath::Abs(Feet(Pursuer).Z-Feet(Character).Z)<1500*WorldUnitsPerOriginalUnit; }
void USnowGnorcBehaviorComponent::Face(float TurnUnits,float DeadZoneUnits)
{
    if (!IsValid(Pursuer)) return;
    const float Desired=FMath::RoundToFloat((Pursuer->GetActorLocation()-Character->GetActorLocation()).Rotation().Yaw*256.f/360.f)*360.f/256.f;
    const float Error=FMath::FindDeltaAngleDegrees(Heading,Desired);
    if (FMath::Abs(Error)<=DeadZoneUnits*360.f/256.f) return;
    Heading=FMath::UnwindDegrees(Heading+FMath::Clamp(Error,-TurnUnits*360.f/256.f,TurnUnits*360.f/256.f));
}
void USnowGnorcBehaviorComponent::UpdateBodyCollision()
{
    ToastyEncounterCollision::FitBody(Mesh,BodyCollision,ChargeSensor,false);
    HitPlayer();
}
void USnowGnorcBehaviorComponent::StepOriginal()
{
    PreviousLocation=Character->GetActorLocation(); PreviousHeading=Heading; ++SimulationTicks;
    if (bFirstTick)
    {
        ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
        SnowGnorc::Bind(Damageable,TEXT("Call Deal_Damage"),WalkingAI,TEXT("On Damaged"),true);
        SnowGnorc::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USnowGnorcBehaviorComponent,OnAcceptedDamage));
        bFirstTick=false;
    }
    TakeMovementControl();
    if (!IsValid(Pursuer)) Pursuer=UGameplayStatics::GetPlayerCharacter(this,0);
    if (State==ESnowGnorcState::Dead) return;
    const bool Complete=AdvanceAnimation();
    if (State==ESnowGnorcState::Dying)
    {
        if (DeathSpeed>0)
        {
            FCollisionQueryParams Q(SCENE_QUERY_STAT(SnowGnorcDeath),false,Character); SnowGnorc::IgnorePlayer(Q,Pursuer);
            const FVector Start=Character->GetActorLocation(),Delta=DeathDirection*DeathSpeed*WorldUnitsPerOriginalUnit;
            FHitResult Hit; auto* Capsule=Character->GetCapsuleComponent();
            GetWorld()->SweepSingleByChannel(Hit,Start,Start+Delta,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1),Q);
            Character->SetActorLocation(Start+Delta*(Hit.bBlockingHit?FMath::Max(0.f,Hit.Time-.002f):1.f),false,nullptr,ETeleportType::TeleportPhysics);
            DeathSpeed=FMath::Max(0.f,DeathSpeed-16);
        }
        if (Complete && CurrentClip==6) FinishCorpse(); return;
    }
    switch (State)
    {
    case ESnowGnorcState::Idle:
        if (Complete)
        {
            if (CurrentClip==8) SelectClip(9);
            else if (IdleCycles==0) { IdleCycles=FMath::RandRange(2,4); SelectClip(8); }
            else { --IdleCycles; SelectClip(0); }
        }
        Face(3,16);
        if (Distance()<7168 && ComparableHeight()) { State=ESnowGnorcState::Alert; SelectClip(2); }
        break;
    case ESnowGnorcState::Alert:
        if (Complete && CurrentClip==2) { State=ESnowGnorcState::Guarding; SelectClip(3,false); }
        break;
    case ESnowGnorcState::Guarding:
        Face(3);
        if (Distance()<3300 && ComparableHeight())
        {
            State=ESnowGnorcState::Punching; bHitThisAttack=false; RecoveryTicks=0; ++PunchCount; SelectClip(4);
            PendingEnemyEvents.Add(ESpyroEnemySignal::AttackCommitted,4);
        }
        else if (Distance()>8192) { State=ESnowGnorcState::LoweringGuard; SelectClip(5); }
        break;
    case ESnowGnorcState::Punching:
        Face(6); HitPlayer();
        if (Complete && CurrentClip==4)
        {
            if (RecoveryTicks>0) { State=ESnowGnorcState::Recovering; SelectClip(7,false); }
            else { State=ESnowGnorcState::Idle; SelectClip(0); }
        }
        break;
    case ESnowGnorcState::Recovering:
        if (Complete && CurrentClip==7 && --RecoveryTicks<=0) { State=ESnowGnorcState::Idle; SelectClip(0); }
        break;
    case ESnowGnorcState::LoweringGuard:
        if (Complete && CurrentClip==5) { State=ESnowGnorcState::Idle; SelectClip(0,false); }
        break;
    default: break;
    }
}
void USnowGnorcBehaviorComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction); if (!Character || !Character->HasAuthority()) return;
    if (UGameplayStatics::IsGamePaused(this) || SnowGnorc::Bool(Damageable,TEXT("Frozen")) || SnowGnorc::Bool(Damageable,TEXT("Paralyzed_by_Fear")) || SnowGnorc::Bool(Dropper,TEXT("Reset_in_Progress"))) { PendingEnemyEvents.Flush(this, OnEnemySignal); return; }
    Accumulator+=FMath::Max(0.f,DeltaTime); int32 Steps=0;
    while (Accumulator+KINDA_SMALL_NUMBER>=GnorcStep && Steps++<30) { Accumulator=FMath::Max(0.f,Accumulator-GnorcStep); StepOriginal(); }
    Accumulator=FMath::Min(Accumulator,GnorcStep); const float Alpha=Accumulator/GnorcStep;
    Mesh->SetWorldLocation(FMath::Lerp(PreviousLocation,Character->GetActorLocation(),Alpha)+Home.TransformVector(MeshOffset));
    Mesh->SetWorldRotation(FRotator(0,PreviousHeading+FMath::FindDeltaAngleDegrees(PreviousHeading,Heading)*Alpha-90,0));
    PendingEnemyEvents.Flush(this, OnEnemySignal);
}

void USnowGnorcBehaviorComponent::HitPlayer()
{
    if (!Character || !Character->HasAuthority() || bDefeated || bHitThisAttack || State!=ESnowGnorcState::Punching || !IsValid(Pursuer) ||
        UGameplayStatics::IsGamePaused(this) || SnowGnorc::Bool(Damageable,TEXT("Frozen")) || SnowGnorc::Bool(Damageable,TEXT("Paralyzed_by_Fear")) || SnowGnorc::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    // Collision selects NEXT frame whenever interpolation progress is nonzero.
    const int32 Clip=Progress>0?NextClip:CurrentClip,Frame=Progress>0?NextFrame:CurrentFrame;
    if (Clip!=4 || Frame<7 || Frame>18) return;
    // Class 198 group 1: broad radius 3200, primitive 0x0c800106,
    // zero vertical offset. func_8004E3C8 interprets type 1 as a sphere;
    // flags 6 apply player damage without blocking or damaging the Gnorc.
    // Adapt the query radius to Spyro's UE capsule; retain physical wall checks.
    const auto* Player=Cast<ACharacter>(Pursuer); if (!Player) return;
    const float Radius=3200*WorldUnitsPerOriginalUnit+Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
    if (FVector::DistSquared(Feet(Character),Pursuer->GetActorLocation())>=Radius*Radius ||
        !SpyroMeleeContact::HasClearContact(Character,Pursuer)) return;
    UFunction* F=Pursuer->FindFunction(TEXT("Deal Damage to Player")); if (!F) return;
    FStructOnScope Params(F); UObject* D=SnowGnorc::ObjectValue(Pursuer,TEXT("Damageable")); const int32 Before=SnowGnorc::Number(D,TEXT("Hit Points"));
    for (TFieldIterator<FProperty> It(F);It && It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        void* V=It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
        if (auto* P=CastField<FByteProperty>(*It))
        {
            // Use the same Crush reaction as the Sleeping Dog: Spyro flattens.
            const int64 CrushDamage=P->Enum?P->Enum->GetValueByNameString(TEXT("Damage_Types::NewEnumerator2")):INDEX_NONE;
            if (CrushDamage<0 || CrushDamage>MAX_uint8) return;
            P->SetPropertyValue(V,static_cast<uint8>(CrushDamage));
        }
        if (auto* P=CastField<FStructProperty>(*It)) if (P->Struct==TBaseStructure<FVector>::Get()) *static_cast<FVector*>(V)=(Pursuer->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D();
        if (auto* P=CastField<FObjectPropertyBase>(*It)) P->SetObjectPropertyValue(V,Character);
    }
    Pursuer->ProcessEvent(F,Params.GetStructMemory());
    if (SnowGnorc::Number(D,TEXT("Hit Points"))<Before) { bHitThisAttack=true; RecoveryTicks=3; ++AcceptedPlayerHits; }
}
void USnowGnorcBehaviorComponent::OnChargeSensorOverlap(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit)
{
    if (!bDefeated && Other && Other!=Character && OtherComponent && OtherComponent->GetCollisionObjectType()==ECC_GameTraceChannel4)
    { auto* Capsule=Character->GetCapsuleComponent(); Capsule->OnComponentBeginOverlap.Broadcast(Capsule,Other,OtherComponent,BodyIndex,bSweep,Hit); }
}

void USnowGnorcBehaviorComponent::OnAcceptedDamage()
{
    if (!Character->HasAuthority() || bDefeated || SnowGnorc::Bool(Damageable,TEXT("Invincible")) || SnowGnorc::Bool(Damageable,TEXT("Frozen")) || SnowGnorc::Bool(Dropper,TEXT("Reset_in_Progress"))) return;
    if (SnowGnorc::Number(Damageable,TEXT("Deal Damage - Damage Type"))!=2) return;
    SnowGnorc::SetNumber(Damageable,TEXT("Hit Points"),0); Defeat();
}
void USnowGnorcBehaviorComponent::Defeat()
{
    bDefeated=true; State=ESnowGnorcState::Dying; DeathSpeed=240; DeathDirection=IsValid(Pursuer)?Pursuer->GetActorForwardVector():FRotator(0,Heading+180,0).Vector(); bHitThisAttack=true;
    DropGemRange(0,1); const bool Suppressed=SnowGnorc::Bool(Dropper,TEXT("Cannot_Drop_Items")); SnowGnorc::SetBool(Dropper,TEXT("Cannot_Drop_Items"),true);
    if (auto* P=CastField<FMulticastDelegateProperty>(SnowGnorc::Property(Damageable,TEXT("Damage Was Successfully Dealt"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(Damageable))) D->ProcessMulticastDelegate<UObject>(nullptr);
    SnowGnorc::SetBool(Dropper,TEXT("Cannot_Drop_Items"),Suppressed); TakeMovementControl(); SelectClip(6);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void USnowGnorcBehaviorComponent::FinishCorpse()
{
    if (bCorpseFinished) return; bCorpseFinished=true; State=ESnowGnorcState::Dead; StopSounds();
    if (DeathSmoke)
        DeathSmokeInstance=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),DeathSmoke,
            Feet(Character)+FVector(0,0,60),FRotator::ZeroRotator,FVector(.5f),true);
    Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false);
    if (auto* P=CastField<FMulticastDelegateProperty>(SnowGnorc::Property(WalkingAI,TEXT("Corpse Poofed"))))
        if (auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(WalkingAI))) D->ProcessMulticastDelegate<UObject>(nullptr);
}

void USnowGnorcBehaviorComponent::OnDropperReset()
{
    if (IsValid(DeathSmokeInstance)) DeathSmokeInstance->DestroyComponent();
    DeathSmokeInstance=nullptr;
    PendingEnemyEvents.Reset(); StopSounds(); SoundCueHistory.Reset(); ReleasedGemIndices.Reset(); GemsSpawned=0;
    State=ESnowGnorcState::Idle; IdleCycles=RecoveryTicks=0; bDefeated=bHitThisAttack=bCorpseFinished=false;
    Accumulator=DeathSpeed=0; AcceptedPlayerHits=PunchCount=0;
    Character->SetActorTransform(Home,false,nullptr,ETeleportType::TeleportPhysics); PreviousLocation=Home.GetLocation(); Heading=PreviousHeading=Home.Rotator().Yaw;
    Character->SetActorHiddenInGame(false); Character->SetActorEnableCollision(true);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ChargeSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ToastyEncounterCollision::Configure(Character,BodyCollision,ChargeSensor);
    SnowGnorc::SetNumber(Damageable,TEXT("Hit Points"),1); TakeMovementControl(); SelectClip(0,false);
    PendingEnemyEvents.Add(ESpyroEnemySignal::ResetCompleted);
}
void USnowGnorcBehaviorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    PendingEnemyEvents.Reset();
    StopSounds();
    if (Mesh) Mesh->UnregisterOnBoneTransformsFinalizedDelegate(BoneTransformsHandle);
    if (ChargeSensor) ChargeSensor->OnComponentBeginOverlap.RemoveDynamic(this,&USnowGnorcBehaviorComponent::OnChargeSensorOverlap);
    SnowGnorc::Bind(Damageable,TEXT("Call Deal_Damage"),this,GET_FUNCTION_NAME_CHECKED(USnowGnorcBehaviorComponent,OnAcceptedDamage),true);
    SnowGnorc::Bind(Dropper,TEXT("Item Dropper Successfully Reset"),this,GET_FUNCTION_NAME_CHECKED(USnowGnorcBehaviorComponent,OnDropperReset),true);
    Super::EndPlay(Reason);
}
void USnowGnorcBehaviorComponent::DropGemRange(int32 First, int32 Count)
{
    if (SnowGnorc::Bool(Dropper, TEXT("Cannot_Drop_Items"))) return;
    SnowGnorc::Call(Dropper, TEXT("Remove Gems We Perma Collected"));
    SnowGnorc::Call(Dropper, TEXT("Find All Items I Have But Shouldn't Drop"));
    auto* ItemsProperty = CastField<FArrayProperty>(SnowGnorc::Property(Dropper, TEXT("Items_to_Drop")));
    auto* PendingProperty = CastField<FArrayProperty>(SnowGnorc::Property(Dropper, TEXT("Items_I_Have_But_Shouldnt_Drop")));
    auto* SpawnedProperty = CastField<FArrayProperty>(SnowGnorc::Property(Dropper, TEXT("Items_I_Dropped")));
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
        const FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, SnowGnorc::Number(Dropper, TEXT("Item_Spawn_Height_Offset")));
        AActor* Gem = GetWorld()->SpawnActor<AActor>(GemClass, Location, GetOwner()->GetActorRotation(), Spawn);
        if (!Gem) continue;
        UFunction* Initialize = Gem->FindFunction(TEXT("Gem_Spawn_Process"));
        if (!Initialize) { Gem->Destroy(); continue; }
        FStructOnScope Params(Initialize);
        for (TFieldIterator<FProperty> It(Initialize); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            const FString Name = SnowGnorc::Key(It->GetName());
            void* Value = It->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
            if (auto* P = CastField<FObjectPropertyBase>(*It))
            {
                if (Name == TEXT("playerwhospawnedme")) P->SetObjectPropertyValue(Value, SnowGnorc::ObjectValue(Damageable, TEXT("Deal Damage - Person Who Dealt the Damage")));
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
