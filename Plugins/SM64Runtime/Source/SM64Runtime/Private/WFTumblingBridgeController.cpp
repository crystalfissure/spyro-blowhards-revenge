#include "WFTumblingBridgeController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AWFTumblingBridgeController::AWFTumblingBridgeController()
{
    PrimaryActorTick.bCanEverTick = true;
    WholeBridgeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WholeBridgeMesh"));
    RootComponent = WholeBridgeMesh;
    WholeBridgeMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PieceClass = ASM64MovingPlatformBase::StaticClass();
}

void AWFTumblingBridgeController::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    WholeBridgeMesh->SetStaticMesh(WholeBridgeAsset ? WholeBridgeAsset : PieceMesh);
    WholeBridgeMesh->SetVisibility(WholeBridgeAsset != nullptr, true);
    WholeBridgeMesh->SetCollisionEnabled(
        WholeBridgeAsset ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AWFTumblingBridgeController::BeginPlay()
{
    Super::BeginPlay();
    ResetBridge();
}

void AWFTumblingBridgeController::ResetForAct_Implementation()
{
    ResetBridge();
}

void AWFTumblingBridgeController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn)
    {
        return;
    }
    const float Distance = FVector::Dist(Pawn->GetActorLocation(), GetActorLocation());
    if (!bPiecesActive && Distance < SpawnDistance)
    {
        SpawnPieces();
    }
    else if (bPiecesActive && Distance > ResetDistance)
    {
        ResetBridge();
    }
}

void AWFTumblingBridgeController::SpawnPieces()
{
    if (bPiecesActive || !PieceClass)
    {
        return;
    }
    bPiecesActive = true;
    WholeBridgeMesh->SetVisibility(false, true);
    WholeBridgeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pieces.Reset();
    for (int32 Index = 0; Index < PieceCount; ++Index)
    {
        FTransform PieceTransform = GetActorTransform();
        FVector Location = PieceTransform.GetLocation();
        Location.Y += FirstPieceOffset + PieceSpacing * Index;
        PieceTransform.SetLocation(Location);
        ASM64MovingPlatformBase* Piece = GetWorld()->SpawnActorDeferred<ASM64MovingPlatformBase>(
            PieceClass,
            PieceTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Piece)
        {
            Piece->Motion = ESM64PlatformMotion::TumblingPiece;
            Piece->DefaultMesh = PieceMesh;
            Piece->DefaultCollisionMesh = PieceCollisionMesh;
            Piece->StableId = FName(*FString::Printf(TEXT("WF_TumblingBridge_%02d"), Index));
            Piece->InitialPhaseFrames = 0;
            UGameplayStatics::FinishSpawningActor(Piece, PieceTransform);
            Pieces.Add(Piece);
        }
    }
}

void AWFTumblingBridgeController::ResetBridge()
{
    for (TWeakObjectPtr<ASM64MovingPlatformBase>& Piece : Pieces)
    {
        if (Piece.IsValid())
        {
            Piece->Destroy();
        }
    }
    Pieces.Reset();
    bPiecesActive = false;
    WholeBridgeMesh->SetVisibility(true, true);
    WholeBridgeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}
