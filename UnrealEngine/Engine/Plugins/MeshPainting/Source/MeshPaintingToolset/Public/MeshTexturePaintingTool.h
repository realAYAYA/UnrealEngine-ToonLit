// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/BaseBrushTool.h"
#include "InteractiveTool.h"
#include "Delegates/DelegateCombinations.h"
#include "Components/MeshComponent.h"
#include "MeshPaintHelpers.h"
#include "MeshPaintingToolsetTypes.h"
#include "MeshVertexPaintingTool.h"
#include "TexturePaintToolset.h"
#include "MeshTexturePaintingTool.generated.h"


struct FToolBuilderState;
struct FPaintTexture2DData;
class UTexture2D;
class UTextureRenderTarget2D;
struct FTextureTargetListInfo;
enum class EMeshPaintModeAction : uint8;
class IMeshPaintComponentAdapter;

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
	void PaintTexture(FMeshPaintParameters& InParams, TArray<FTexturePaintTriangleInfo>& InInfluencedTriangles, const IMeshPaintComponentAdapter& GeometryInfo);
	void FinishPaintingTexture();

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
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UMeshTexturePaintingToolProperties> TextureProperties;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UTexture>> Textures;

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
	TMap< TObjectPtr<UTexture2D>, FPaintTexture2DData > PaintTargetData;

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
};
