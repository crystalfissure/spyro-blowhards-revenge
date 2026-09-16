#include "SpyroEditorLibrary.h"

#include "ChargeWobbleComponent.h"

#include "Animation/AnimSequence.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

bool USpyroEditorLibrary::AddChargeWobbleComponent(
    UBlueprint* Blueprint,
    FName ComponentVariableName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return false;
    }
    for (USCS_Node* ExistingNode : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (ExistingNode &&
            (ExistingNode->GetVariableName() == ComponentVariableName ||
             ExistingNode->ComponentClass == UChargeWobbleComponent::StaticClass()))
        {
            return true;
        }
    }
    Blueprint->Modify();
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        UChargeWobbleComponent::StaticClass(), ComponentVariableName);
    if (!NewNode)
    {
        return false;
    }
    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool USpyroEditorLibrary::ConfigureChargeWobble(
    UBlueprint* Blueprint,
    UAnimSequence* RestAnimation,
    UAnimSequence* ReactionAnimation)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !RestAnimation || !ReactionAnimation)
    {
        return false;
    }
    bool bConfigured = false;
    Blueprint->Modify();
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        UChargeWobbleComponent* Wobble = Node
            ? Cast<UChargeWobbleComponent>(Node->ComponentTemplate) : nullptr;
        if (!Wobble)
        {
            continue;
        }
        Node->Modify();
        Wobble->Modify();
        Wobble->RestAnimation = RestAnimation;
        Wobble->ReactionAnimation = ReactionAnimation;
        bConfigured = true;
    }
    if (bConfigured)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return bConfigured;
}

bool USpyroEditorLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}
