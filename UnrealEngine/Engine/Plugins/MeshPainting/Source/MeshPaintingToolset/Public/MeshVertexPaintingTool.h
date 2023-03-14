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
#include "MeshPaintInteractions.h"
#include "MeshVertexPaintingTool.generated.h"


struct FToolBuilderState;
enum class EMeshPaintModeAction : uint8;
class IMeshPaintComponentAdapter;

struct FPaintRayResults
{
	FMeshPaintParameters Params;
	FHitResult BestTraceResult;
};

UENUM()
enum class EMeshPaintWeightTypes : uint8
{
	/** Lerp Between Two Textures using Alpha Value */
	AlphaLerp = 2 UMETA(DisplayName = "Alpha (Two Textures)"),

	/** Weighting Three Textures according to Channels*/
	RGB = 3 UMETA(DisplayName = "RGB (Three Textures)"),

	/**  Weighting Four Textures according to Channels*/
	ARGB = 4 UMETA(DisplayName = "ARGB (Four Textures)"),

	/**  Weighting Five Textures according to Channels */
	OneMinusARGB = 5 UMETA(DisplayName = "ARGB - 1 (Five Textures)")
};

UENUM()
enum class EMeshPaintTextureIndex : uint8
{
	TextureOne = 0,
	TextureTwo,
	TextureThree,
	TextureFour,
	TextureFive
};

DECLARE_DELEGATE_RetVal(TArray<UMeshComponent*>, FGetSelectedMeshComponents);
/**
 *
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshColorPaintingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class MESHPAINTINGTOOLSET_API UMeshWeightPaintingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class MESHPAINTINGTOOLSET_API UMeshVertexPaintingToolProperties : public UBrushBaseProperties
{
	GENERATED_BODY()

public:
	UMeshVertexPaintingToolProperties();

	/** Color used for Applying Vertex Color Painting */
	UPROPERTY(EditAnywhere, Category = VertexPainting)
	FLinearColor PaintColor;

	/** Color used for Erasing Vertex Color Painting */
	UPROPERTY(EditAnywhere, Category = VertexPainting)
	FLinearColor EraseColor;

	/** Enables "Flow" painting where paint is continually applied from the brush every tick */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Enable Brush Flow"))
	bool bEnableFlow;

	/** Whether back-facing triangles should be ignored */
	UPROPERTY(EditAnywhere, Category = Brush, meta = (DisplayName = "Ignore Back-Facing"))
	bool bOnlyFrontFacingTriangles;

	/** Size of vertex points drawn when mesh painting is active. */
	UPROPERTY(EditAnywhere, Category = "VertexPainting|Visualization")
	float VertexPreviewSize;
};


UCLASS()
class MESHPAINTINGTOOLSET_API UMeshColorPaintingToolProperties : public UMeshVertexPaintingToolProperties
{
	GENERATED_BODY()

public:
	UMeshColorPaintingToolProperties();


	/** Whether or not to apply Vertex Color Painting to the Red Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, DisplayName = "Red")
	bool bWriteRed;

	/** Whether or not to apply Vertex Color Painting to the Green Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, DisplayName = "Green")
	bool bWriteGreen;

	/** Whether or not to apply Vertex Color Painting to the Blue Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, DisplayName = "Blue")
	bool bWriteBlue;

	/** Whether or not to apply Vertex Color Painting to the Alpha Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, DisplayName = "Alpha")
	bool bWriteAlpha;

	/** When unchecked the painting on the base LOD will be propagate automatically to all other LODs when exiting the mode or changing the selection */
	UPROPERTY(EditAnywhere, Category = Painting, meta = (TransientToolProperty))
	bool bPaintOnSpecificLOD;

	/** LOD Index to which should specifically be painted */
	UPROPERTY(EditAnywhere, Category = Painting, meta = (UIMin = "0", ClampMin = "0", EditCondition = "bPaintOnSpecificLOD", TransientToolProperty))
	int32 LODIndex;
};

UCLASS()
class MESHPAINTINGTOOLSET_API UMeshWeightPaintingToolProperties : public UMeshVertexPaintingToolProperties
{
	GENERATED_BODY()

public:
	UMeshWeightPaintingToolProperties();

	/** Texture Blend Weight Painting Mode */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	EMeshPaintWeightTypes TextureWeightType;

	/** Texture Blend Weight index which should be applied during Painting */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	EMeshPaintTextureIndex PaintTextureWeightIndex;

	/** Texture Blend Weight index which should be erased during Painting */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	EMeshPaintTextureIndex EraseTextureWeightIndex;
};

UCLASS(Abstract)
class MESHPAINTINGTOOLSET_API UMeshVertexPaintingTool : public UBaseBrushTool, public IMeshPaintSelectionInterface
{
	GENERATED_BODY()

public:
	UMeshVertexPaintingTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override;
	virtual bool CanAccept() const override;
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual	void OnUpdateModifierState(int ModifierID, bool bIsOn) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual	bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool IsPainting() const
	{
		return bArePainting;
	}
	virtual double EstimateMaximumTargetDimension() override;

	FSimpleDelegate& OnPaintingFinished()
	{
		return OnPaintingFinishedDelegate;
	}
	virtual bool IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const override;
	virtual bool AllowsMultiselect() const override
	{
		return true;
	}

protected:
	virtual void SetAdditionalPaintParameters(FMeshPaintParameters& InPaintParameters) {};
	virtual void FinishPainting();
	void UpdateResult();
	double CalculateTargetEdgeLength(int TargetTriCount);
	bool Paint(const FVector& InRayOrigin, const FVector& InRayDirection);
	bool Paint(const TArrayView<TPair<FVector, FVector>>& Rays);
	virtual void CacheSelectionData() {};
	/** Per vertex action function used for painting vertex data */
	void ApplyVertexData(FPerVertexPaintActionArgs& InArgs, int32 VertexIndex, FMeshPaintParameters Parameters);


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
	TObjectPtr<UMeshVertexPaintingToolProperties> VertexProperties;

	/** Flag for whether or not we are currently painting */
	bool bArePainting;
	bool bDoRestoreRenTargets;
	/** Time kept since the user has started painting */
	float TimeSinceStartedPainting;
	/** Overall time value kept for drawing effects */
	float Time;
	FHitResult LastBestHitResult;
	FSimpleDelegate OnPaintingFinishedDelegate;
};

UCLASS()
class MESHPAINTINGTOOLSET_API UMeshColorPaintingTool : public UMeshVertexPaintingTool
{
	GENERATED_BODY()

public:
	UMeshColorPaintingTool();
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	void LODPaintStateChanged(const bool bLODPaintingEnabled);
	int32 GetMaxLODIndexToPaint() const;
	void PaintLODChanged();
	int32 GetCachedLODIndex() const
	{
		return CachedLODIndex;
	}
	void CycleMeshLODs(int32 Direction);

protected:
	virtual void CacheSelectionData() override;
	virtual void SetAdditionalPaintParameters(FMeshPaintParameters& InPaintParameters) override;
	void ApplyForcedLODIndex(int32 ForcedLODIndex);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMeshColorPaintingToolProperties> ColorProperties;

	/** Current LOD index used for painting / forcing */
	int32 CachedLODIndex;
	/** Whether or not a specific LOD level should be forced */
	bool bCachedForceLOD;
};


UCLASS()
class MESHPAINTINGTOOLSET_API UMeshWeightPaintingTool : public UMeshVertexPaintingTool
{
	GENERATED_BODY()

public:
	UMeshWeightPaintingTool();
	virtual void Setup() override;

protected:
	virtual void CacheSelectionData() override;
	virtual void SetAdditionalPaintParameters(FMeshPaintParameters& InPaintParameters);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMeshWeightPaintingToolProperties> WeightProperties;
};
