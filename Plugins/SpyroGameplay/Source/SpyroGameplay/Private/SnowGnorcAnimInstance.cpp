#include "SnowGnorcAnimInstance.h"
#include "SnowGnorcBehaviorComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Actor.h"

// Samples the two original keyframes; the animation worker only reads proxy-owned inputs.
struct FSnowGnorcAnimProxy : FAnimInstanceProxy
{
    explicit FSnowGnorcAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* A = nullptr;
    UAnimSequence* B = nullptr;
    float TimeA = 0.f, TimeB = 0.f, Alpha = 0.f;
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        A = B = nullptr;
        if (AActor* Owner = Instance->GetOwningActor())
            if (auto* Behavior = Owner->FindComponentByClass<USnowGnorcBehaviorComponent>())
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
FAnimInstanceProxy* USnowGnorcAnimInstance::CreateAnimInstanceProxy() { return new FSnowGnorcAnimProxy(this); }
void USnowGnorcAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
