// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultiSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "BakeMeshAttributeMapsToolBase.h"

// Render Capture algorithm includes
#include "Scene/SceneCapturePhotoSet.h"

#include "BakeRenderCaptureTool.generated.h"

class UTexture2D;

//
// Tool Result
//


UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeRenderCaptureResults  : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> BaseColorMap;

	/** World space normal map */
	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> NormalMap;

	/** Packed Metallic/Roughness/Specular Map */
	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty, DisplayName = "Packed MRS Map"))
	TObjectPtr<UTexture2D> PackedMRSMap;

	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> MetallicMap;

	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> RoughnessMap;

	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> SpecularMap;

	UPROPERTY(VisibleAnywhere, Category = Results, meta = (TransientToolProperty))
	TObjectPtr<UTexture2D> EmissiveMap;
};


//
// Tool Builder
//



UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeRenderCaptureToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};



//
// Tool Properties
//


UCLASS()
class MESHMODELINGTOOLSEXP_API URenderCaptureProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault, ClampMin = "1", UIMin= "1"), DisplayName="Render Capture Resolution")
	EBakeTextureResolution Resolution = EBakeTextureResolution::Resolution512;

	// Whether to generate a texture for the Base Color property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault))
	bool bBaseColorMap = false;

	// Whether to generate a texture for the World Normal property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault))
	bool bNormalMap = false;

	// Whether to generate a packed texture with Metallic, Roughness and Specular properties
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault), DisplayName="Packed MRS Map")
	bool bPackedMRSMap = false;

	// Whether to generate a texture for the Metallic property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault, EditCondition="bPackedMRSMap == false"))
	bool bMetallicMap = false;

	// Whether to generate a texture for the Roughness property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault, EditCondition="bPackedMRSMap == false"))
	bool bRoughnessMap = false;

	// Whether to generate a texture for the Specular property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault, EditCondition="bPackedMRSMap == false"))
	bool bSpecularMap = false;
	
	// Whether to generate a texture for the Emissive property
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, meta = (NoResetToDefault))
	bool bEmissiveMap = false;

	// Whether to use anti-aliasing in the render captures, this may introduce artefacts if pixels at different scene depths get blended
	UPROPERTY(Category = RenderCaptureOptions, EditAnywhere, AdvancedDisplay)
	bool bAntiAliasing = false;

	// These are hidden in the UI right now, we might want to expose them if they turn out to be useful for very large
	// or very small objects (not tested yet) TODO Figure out if we want to expose these options
	
	UPROPERTY(meta = (ClampMin = "5.0", ClampMax = "160.0"))
	float CaptureFieldOfView = 30.0f;

	UPROPERTY(meta = (ClampMin = "0.001", ClampMax = "1000.0"))
	float NearPlaneDist = 1.0f;
	
	UPROPERTY()
	bool bDeviceDepthMap = false;

	bool operator==(const URenderCaptureProperties& Other) const
	{
		// Intentionally omitted TransientToolProperty
		return Resolution == Other.Resolution
			&& bBaseColorMap == Other.bBaseColorMap
			&& bNormalMap == Other.bNormalMap
			&& bMetallicMap == Other.bMetallicMap
			&& bRoughnessMap == Other.bRoughnessMap
			&& bSpecularMap == Other.bSpecularMap
			&& bPackedMRSMap == Other.bPackedMRSMap
			&& bEmissiveMap == Other.bEmissiveMap
			&& bAntiAliasing == Other.bAntiAliasing
			&& CaptureFieldOfView == Other.CaptureFieldOfView
			&& NearPlaneDist == Other.NearPlaneDist
			&& bDeviceDepthMap == Other.bDeviceDepthMap;
	}

	bool operator != (const URenderCaptureProperties& Other) const
	{
		return !(*this == Other);
	}
};

UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeRenderCaptureToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	
	/** The map type to preview */
	UPROPERTY(EditAnywhere, Category = BakeOutput, meta = (DisplayName="Preview Output Type", TransientToolProperty, GetOptions = GetMapPreviewNamesFunc))
	FString MapPreview;
	
	UFUNCTION()
	const TArray<FString>& GetMapPreviewNamesFunc()
	{
		return MapPreviewNamesList;
	}
	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> MapPreviewNamesList;

	/** Number of samples per pixel */
	UPROPERTY(EditAnywhere, Category = BakeOutput)
	EBakeTextureSamplesPerPixel SamplesPerPixel = EBakeTextureSamplesPerPixel::Sample1;

	/* Size of generated textures */
	UPROPERTY(EditAnywhere, Category = BakeOutput, meta = (ClampMin="1", UIMin="1"), DisplayName="Output Resolution")
	EBakeTextureResolution TextureSize = EBakeTextureResolution::Resolution512;

	/**
	 * This threshold is used to detect occlusion artefacts (e.g., silhouettes/blotches in the base color) so that texels
	 * exhibiting them can be cleaned up i.e., rejected and filled in using the values of nearby valid texels instead.
	 * 
	 * If the threshold is zero, the cleanup step is skipped.
	 * If the threshold is too large, texels with artefacts won't be detected and the cleanup step is effectively skipped.
	 * If the threshold is too small, texels without artefacts get detected and results will be bad because there won't
	 * be enough nearby valid texels from which to infill values.
	 * 
	 * A good starting point is to choose a threshold around the size of the distance (in world space) between the
	 * target and source meshes; if the target was generated by VoxWrap then the voxel size estimates this distance.
	 */
	UPROPERTY(EditAnywhere, Category = BakeOutput, meta = (ClampMin="0", UIMin="0"), DisplayName="Cleanup Threshold")
	float ValidSampleDepthThreshold = 0.f;
};


UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeRenderCaptureInputToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Target mesh to sample to */
	UPROPERTY(VisibleAnywhere, Category = BakeInput, DisplayName = "Target Mesh", meta = (TransientToolProperty, NoResetToDefault))
	TObjectPtr<UStaticMesh> TargetStaticMesh = nullptr;

	/** UV channel to use for the target mesh */
	UPROPERTY(EditAnywhere, Category = BakeInput, meta = (DisplayName = "Target Mesh UV Channel", GetOptions = GetTargetUVLayerNamesFunc, NoResetToDefault))
	FString TargetUVLayer;

	UFUNCTION()
	int32 GetTargetUVLayerIndex() const
	{
		return TargetUVLayerNamesList.IndexOfByKey(TargetUVLayer);
	}

	UFUNCTION()
	const TArray<FString>& GetTargetUVLayerNamesFunc() const
	{
		return TargetUVLayerNamesList;
	}

	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> TargetUVLayerNamesList;
};


//
// Tool
//



UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeRenderCaptureTool : public UBakeMeshAttributeMapsToolBase
{
	GENERATED_BODY()

public:
	UBakeRenderCaptureTool() = default;

	// Begin UInteractiveTool interface
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	// End UInteractiveTool interface

	// Begin IGenericDataOperatorFactory interface
	virtual TUniquePtr<UE::Geometry::TGenericDataOperator<UE::Geometry::FMeshMapBaker>> MakeNewOperator() override;
	// End IGenericDataOperatorFactory interface

protected:

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;

	UPROPERTY()
	TObjectPtr<UBakeRenderCaptureToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<URenderCaptureProperties> RenderCaptureProperties;

	UPROPERTY()
	TObjectPtr<UBakeRenderCaptureInputToolProperties> InputMeshSettings;

	// The computed textures are displayed in the details panel and used in the preview material, they are written
	// out to assest on shutdown.
	UPROPERTY()
	TObjectPtr<UBakeRenderCaptureResults> ResultSettings;
	
protected:

	// Begin UBakeMeshAttributeMapsToolBase interface
	virtual void UpdateResult() override;
	virtual void UpdateVisualization() override;
	virtual void GatherAnalytics(FBakeAnalytics::FMeshSettings& Data) override;
	virtual FString GetAnalyticsEventName() const override { return TEXT("BakeRC"); }
	// End UBakeMeshAttributeMapsToolBase interface
	
	void InvalidateResults();

	// In this tool we don't call UBakeMeshAttributeMapsToolBase::OnMapsUpdated because it would require e.g, adding
	// the render capture channels to EBakeMapType.  The implementation is simpler and leads to less coupling if we
	// just implement custom versions of the following functions.
	void InvalidateComputeRC();
	void OnMapsUpdatedRC(const TUniquePtr<UE::Geometry::FMeshMapBaker>& NewResult);
	
	/**
	 * Create texture assets from our result map of Texture2D
	 * @param SourceWorld the source world to define where the texture assets will be stored.
	 * @param SourceAsset if not null, result textures will be stored adjacent to this asset.
	 */
	void CreateTextureAssetsRC(UWorld* SourceWorld, UObject* SourceAsset);

	// empty maps are shown when nothing is computed
	UPROPERTY()
	TObjectPtr<UTexture2D> EmptyEmissiveMap;
	UPROPERTY()
	TObjectPtr<UTexture2D> EmptyPackedMRSMap;
	UPROPERTY()
	TObjectPtr<UTexture2D> EmptyRoughnessMap;
	UPROPERTY()
	TObjectPtr<UTexture2D> EmptyMetallicMap;
	UPROPERTY()
	TObjectPtr<UTexture2D> EmptySpecularMap;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialRC;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialPackedRC;
	void InitializePreviewMaterials();

	// TODO We currently need to compute this on the game thread because the implementation has checks for this
	TUniquePtr<UE::Geometry::FSceneCapturePhotoSet> SceneCapture;
	bool bFirstEverSceneCapture = true;

	// If the user cancels a scene capture before the computation completes then the settings which changed to invoke
	// the capture are reverted to these values
	UPROPERTY()
	TObjectPtr<URenderCaptureProperties> ComputedRenderCaptureProperties;
	float ComputedValidDepthThreshold;

	TMap<int, FText> TargetUVLayerToError;

	//
	// Analytics
	//

	void RecordAnalytics() const;
	void GatherAnalytics(const UE::Geometry::FMeshMapBaker& Result);

};