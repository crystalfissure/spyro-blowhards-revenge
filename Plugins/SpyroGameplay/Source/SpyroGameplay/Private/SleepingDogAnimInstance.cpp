#include "SleepingDogAnimInstance.h"
#include "SleepingDogBehaviorComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Actor.h"

// Samples the two original keyframes; the animation worker only reads proxy-owned inputs.
struct FSleepingDogAnimProxy : FAnimInstanceProxy
{
    explicit FSleepingDogAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* A = nullptr;
    UAnimSequence* B = nullptr;
    float TimeA = 0.f, TimeB = 0.f, Alpha = 0.f;
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        A = B = nullptr;
        if (AActor* Owner = Instance->GetOwningActor())
            if (auto* Behavior = Owner->FindComponentByClass<USleepingDogBehaviorComponent>())
                Behavior->GetPoseInputs(A, B, TimeA, TimeB, Alpha);
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        if (!A || !B) return true;
        FPoseContext PoseA(this), PoseB(this);
        PoseA.ResetToRefPose(); PoseB.ResetToRefPose();
        FAnimationPoseData DataA(PoseA), DataB(PoseB), Result(Output);
        A->GetAnimationPose(DataA, FAnimExtractContext(TimeA, false));
        B->GetAnimationPose(DataB, FAnimExtractContext(TimeB, false));
        FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - Alpha, Result);
        return true;
    }
};
FAnimInstanceProxy* USleepingDogAnimInstance::CreateAnimInstanceProxy() { return new FSleepingDogAnimProxy(this); }
void USleepingDogAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
