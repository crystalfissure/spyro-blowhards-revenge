#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "SM64ActActor.h"
#include "SM64MovingPlatformBase.h"
#include "WFTumblingBridgeController.generated.h"

UCLASS(Blueprintable)
class SM64RUNTIME_API AWFTumblingBridgeController : public ASM64ActActor
{
    GENERATED_BODY()

public:
    AWFTumblingBridgeController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void ResetForAct_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SM64|Components")
    UStaticMeshComponent* WholeBridgeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    UStaticMesh* WholeBridgeAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    UStaticMesh* PieceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    UStaticMesh* PieceCollisionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    TSubclassOf<ASM64MovingPlatformBase> PieceClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    int32 PieceCount = 9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    float FirstPieceOffset = -512.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    float PieceSpacing = 128.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    float SpawnDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SM64|Bridge")
    float ResetDistance = 1200.0f;

    UFUNCTION(BlueprintCallable, Category = "SM64|Bridge")
    void SpawnPieces();

    UFUNCTION(BlueprintCallable, Category = "SM64|Bridge")
    void ResetBridge();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    TArray<TWeakObjectPtr<ASM64MovingPlatformBase>> Pieces;
    bool bPiecesActive = false;
};
