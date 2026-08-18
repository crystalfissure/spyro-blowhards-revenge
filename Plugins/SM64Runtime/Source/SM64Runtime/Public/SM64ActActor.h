#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SM64ActActor.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API ASM64ActActor : public AActor
{
    GENERATED_BODY()

public:
    ASM64ActActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Identity")
    FName StableId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Acts", meta = (Bitmask))
    int32 ActMask = 0x3F;

    UFUNCTION(BlueprintCallable, Category = "SM64|Acts")
    virtual void SetCurrentAct(int32 NewAct);

    UFUNCTION(BlueprintPure, Category = "SM64|Acts")
    bool IsEnabledForAct(int32 TestAct) const;

protected:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SM64|Acts")
    bool bActEnabled = true;
};
