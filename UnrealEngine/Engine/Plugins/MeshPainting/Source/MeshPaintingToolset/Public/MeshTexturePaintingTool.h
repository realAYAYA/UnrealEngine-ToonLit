// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/BaseBrushTool.h"
#include "MeshPaintHelpers.h"
#include "MeshPaintInteractions.h"
#include "MeshPaintingToolsetTypes.h"
#include "MeshVertexPaintingTool.h"
#include "Misc/ITransaction.h"

#include "MeshTexturePaintingTool.generated.h"

class UMeshToolManager;
enum class EToolShutdownType : uint8;
struct FTexturePaintMeshSectionInfo;


struct FToolBuilderState;
struct FPaintTexture2DData;
class UTexture2D;
class UTextureRenderTarget2D;
struct FTextureTargetListInfo;
enum class EMeshPaintModeAction : uint8;
class IMeshPaintComponentAdapter;
class FScopedTransaction;

/**
 *
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTexturePaintingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	TWeakObjectPtr<UMeshToolManager> SharedMeshToolData;
};


UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTexturePaintingToolProperties : public UBrushBaseProperties
{
	GENERATED_BODY()

public:
	UMeshTexturePaintingToolProperties();

	/** Color used for Applying Texture Color Painting */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	FLinearColor PaintColor;

	/** Color used for Erasing Texture Color Painting */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	FLinearColor EraseColor;

	/** Whether or not to apply Texture Color Painting to the Red Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Red"))
	bool bWriteRed;

	/** Whether or not to apply Texture Color Painting to the Green Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Green"))
	bool bWriteGreen;

	/** Whether or not to apply Texture Color Painting to the Blue Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Blue"))
	bool bWriteBlue;

	/** Whether or not to apply Texture Color Painting to the Alpha Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Alpha"))
	bool bWriteAlpha;

	/** UV channel which should be used for paint textures */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty))
	int32 UVChannel;

	/** Seam painting flag, True if we should enable dilation to allow the painting of texture seams */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	bool bEnableSeamPainting;

	/** Texture to which Painting should be Applied */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayThumbnail = "true", TransientToolProperty))
	TObjectPtr<UTexture2D> PaintTexture;

	/** Optional Texture Brush to which Painting should use */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayThumbnail = "true", TransientToolProperty))
	TObjectPtr<UTexture2D> PaintBrush = nullptr;

	/** Initial Rotation offset to apply to our paint brush */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty, UIMin = "0.0", UIMax = "360.0", ClampMin = "0.0", ClampMax = "360.0"))
	float PaintBrushRotationOffset = 0.0f;

	/** Whether or not to continously rotate the brush towards the painting direction */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty))
	bool bRotateBrushTowardsDirection = false;

	/** Enables "Flow" painting where paint is continually applied from the brush every tick */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Enable Brush Flow"))
	bool bEnableFlow;

	/** Whether back-facing triangles should be ignored */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Ignore Back-Facing"))
	bool bOnlyFrontFacingTriangles;
};



UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTexturePaintingTool : public UBaseBrushTool, public IMeshPaintSelectionInterface
{
	GENERATED_BODY()

public:
	UMeshTexturePaintingTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override;
	virtual bool CanAccept() const override;
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual	bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual double EstimateMaximumTargetDimension() override;
	virtual bool IsPainting() const
	{
		return bArePainting;
	}


	FSimpleDelegate& OnPaintingFinished()
	{
		return OnPaintingFinishedDelegate;
	}
	void CycleTextures(int32 Direction);

	void CommitAllPaintedTextures();
	void ClearAllTextureOverrides();
	/** Returns the number of texture that require a commit. */
	int32 GetNumberOfPendingPaintChanges() const;

	void FloodCurrentPaintTexture();
	bool ShouldFilterTextureAsset(const FAssetData& AssetData) const;
	void PaintTextureChanged(const FAssetData& AssetData);
	virtual bool IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const override;
	virtual bool AllowsMultiselect() const override
	{
		return false;
	}

protected:
	virtual void SetAdditionalPaintParameters(FMeshPaintParameters& InPaintParameters) {};
	virtual void FinishPainting();
	void UpdateResult();
	double CalculateTargetEdgeLength(int TargetTriCount);
	bool Paint(const FVector& InRayOrigin, const FVector& InRayDirection);
	bool Paint(const TArrayView<TPair<FVector, FVector>>& Rays);
	virtual void CacheSelectionData();
	void CacheTexturePaintData();
	FPaintTexture2DData* GetPaintTargetData(const UTexture2D* InTexture);
	FPaintTexture2DData* AddPaintTargetData(UTexture2D* InTexture);
	void GatherTextureTriangles(IMeshPaintComponentAdapter* Adapter, int32 TriangleIndex, const int32 VertexIndices[3], TArray<FTexturePaintTriangleInfo>* TriangleInfo, TArray<FTexturePaintMeshSectionInfo>* SectionInfos, int32 UVChannelIndex);
	void StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintComponentAdapter& GeometryInfo);
	void PaintTexture(FMeshPaintParameters& InParams, TArray<FTexturePaintTriangleInfo>& InInfluencedTriangles, const IMeshPaintComponentAdapter& GeometryInfo, FMeshPaintParameters* LastParams = nullptr);
	void FinishPaintingTexture();
	void OnTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState);

protected:
	double InitialMeshArea;
	bool bResultValid;
	bool bStampPending;
	bool bInDrag;
	FRay PendingStampRay;
	FRay PendingClickRay;
	FVector2D PendingClickScreenPosition;
	bool bCachedClickRay;

	UPROPERTY(Transient)
	TObjectPtr<UMeshPaintSelectionMechanic> SelectionMechanic;

private:
	bool PaintInternal(const TArrayView<TPair<FVector, FVector>>& Rays, EMeshPaintModeAction PaintAction, float PaintStrength);

	void AddTextureOverrideToComponent(FPaintTexture2DData& TextureData, UMeshComponent* MeshComponent, const IMeshPaintComponentAdapter* MeshPaintAdapter = nullptr);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMeshTexturePaintingToolProperties> TextureProperties;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UTexture>> Textures;

	TArray<FPaintRayResults> LastPaintRayResults;
	bool bRequestPaintBucketFill = false;

	/** Flag for whether or not we are currently painting */
	bool bArePainting;
	bool bDoRestoreRenTargets;
	/** Time kept since the user has started painting */
	float TimeSinceStartedPainting;
	/** Overall time value kept for drawing effects */
	float Time;
	FHitResult LastBestHitResult;
	FSimpleDelegate OnPaintingFinishedDelegate;
	/** Texture paint state */
/** Textures eligible for painting retrieved from the current selection */
	TArray<FPaintableTexture> PaintableTextures;
	/** Cached / stored instance texture paint settings for selected components */
	TMap<UMeshComponent*, FInstanceTexturePaintSettings> ComponentToTexturePaintSettingsMap;

	/** Temporary render target used to draw incremental paint to */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> BrushRenderTargetTexture;

	/** Temporary render target used to store a mask of the affected paint region, updated every time we add incremental texture paint */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> BrushMaskRenderTargetTexture;

	/** Temporary render target used to store generated mask for texture seams, we create this by projecting object triangles into texture space using the selected UV channel */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> SeamMaskRenderTargetTexture;

	/** Stores data associated with our paint target textures */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTexture2D>, FPaintTexture2DData> PaintTargetData;

	/** Store the component overrides active for each paint target textures
	 * Note this is not transactional because we use it as cache of the current state of the scene that we can clean/update after each transaction.
	 */
	UPROPERTY(Transient, NonTransactional)
	TMap<TObjectPtr<UTexture2D>, FPaintComponentOverride> PaintComponentsOverride;

	/** Texture paint: Will hold a list of texture items that we can paint on */
	TArray<FTextureTargetListInfo> TexturePaintTargetList;

	/** Texture paint: The mesh components that we're currently painting */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> TexturePaintingCurrentMeshComponent;

	/** The original texture that we're painting */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PaintingTexture2D;

	/** True if we need to generate a texture seam mask used for texture dilation */
	bool bGenerateSeamMask;

	/** Hold the transaction while we are painting */
	TUniquePtr<FScopedTransaction> PaintingTransaction;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "MeshVertexPaintingTool.h"
#include "TexturePaintToolset.h"
#include "UObject/NoExportTypes.h"
#endif
