// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LandscapeProxy.h"
#include "LandscapeBlueprintBrushBase.h"
#include "Delegates/DelegateCombinations.h"

#include "Landscape.generated.h"

class FTextureRenderTargetResource;
class ULandscapeComponent;
class ILandscapeEdModeInterface;
class SNotificationItem;
class UStreamableRenderAsset;
class FMaterialResource;
struct FLandscapeEditLayerComponentReadbackResult;
struct FLandscapeNotification;
struct FTextureToComponentHelper;
struct FUpdateLayersContentContext;
struct FEditLayersHeightmapMergeParams;
struct FEditLayersWeightmapMergeParams;

namespace ELandscapeToolTargetType
{
	enum Type : int8;
};

namespace EditLayersHeightmapLocalMerge_RenderThread
{
	struct FMergeInfo;
}

namespace EditLayersWeightmapLocalMerge_RenderThread
{
	struct FMergeInfo;
}

#if WITH_EDITOR
extern LANDSCAPE_API TAutoConsoleVariable<int32> CVarLandscapeSplineFalloffModulation;
#endif

UENUM()
enum ELandscapeSetupErrors
{
	LSE_None,
	/** No Landscape Info available. */
	LSE_NoLandscapeInfo,
	/** There was already component with same X,Y. */
	LSE_CollsionXY,
	/** No Layer Info, need to add proper layers. */
	LSE_NoLayerInfo,
	LSE_MAX,
};

UENUM()
enum class ERTDrawingType : uint8
{
	RTAtlas,
	RTAtlasToNonAtlas,
	RTNonAtlasToAtlas,
	RTNonAtlas,
	RTMips
};

UENUM()
enum class EHeightmapRTType : uint8
{
	HeightmapRT_CombinedAtlas,
	HeightmapRT_CombinedNonAtlas,
	HeightmapRT_Scratch1,
	HeightmapRT_Scratch2,
	HeightmapRT_Scratch3,
	// Mips RT
	HeightmapRT_Mip1,
	HeightmapRT_Mip2,
	HeightmapRT_Mip3,
	HeightmapRT_Mip4,
	HeightmapRT_Mip5,
	HeightmapRT_Mip6,
	HeightmapRT_Mip7,
	HeightmapRT_Count
};

UENUM()
enum class EWeightmapRTType : uint8
{
	WeightmapRT_Scratch_RGBA,
	WeightmapRT_Scratch1,
	WeightmapRT_Scratch2,
	WeightmapRT_Scratch3,

	// Mips RT
	WeightmapRT_Mip0,
	WeightmapRT_Mip1,
	WeightmapRT_Mip2,
	WeightmapRT_Mip3,
	WeightmapRT_Mip4,
	WeightmapRT_Mip5,
	WeightmapRT_Mip6,
	WeightmapRT_Mip7,
	
	WeightmapRT_Count
};

#if WITH_EDITOR
enum ELandscapeLayerUpdateMode : uint32;
#endif

USTRUCT()
struct FLandscapeLayerBrush
{
	GENERATED_USTRUCT_BODY()

	FLandscapeLayerBrush()
#if WITH_EDITORONLY_DATA
		: FLandscapeLayerBrush(nullptr)
#endif
	{}

	FLandscapeLayerBrush(ALandscapeBlueprintBrushBase* InBlueprintBrush)
#if WITH_EDITORONLY_DATA
		: BlueprintBrush(InBlueprintBrush)
		, LandscapeSize(MAX_int32, MAX_int32)
		, LandscapeRenderTargetSize(MAX_int32, MAX_int32)
#endif
	{}

#if WITH_EDITOR
	UTextureRenderTarget2D* Render(bool InIsHeightmap, const FIntRect& InLandscapeSize, UTextureRenderTarget2D* InLandscapeRenderTarget, const FName& InWeightmapLayerName = NAME_None);
	LANDSCAPE_API ALandscapeBlueprintBrushBase* GetBrush() const;
	bool IsAffectingHeightmap() const;
	bool IsAffectingWeightmapLayer(const FName& InWeightmapLayerName) const;
	void SetOwner(ALandscape* InOwner);
#endif

private:

#if WITH_EDITOR
	bool Initialize(const FIntRect& InLandscapeExtent, UTextureRenderTarget2D* InLandscapeRenderTarget);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<ALandscapeBlueprintBrushBase> BlueprintBrush;

	FTransform LandscapeTransform;
	FIntPoint LandscapeSize;
	FIntPoint LandscapeRenderTargetSize;
#endif
};

UENUM()
enum ELandscapeBlendMode
{
	LSBM_AdditiveBlend,
	LSBM_AlphaBlend,
	LSBM_MAX,
};

USTRUCT()
struct FLandscapeLayer
{
	GENERATED_USTRUCT_BODY()

	FLandscapeLayer()
		: Guid(FGuid::NewGuid())
		, Name(NAME_None)
		, bVisible(true)
		, bLocked(false)
		, HeightmapAlpha(1.0f)
		, WeightmapAlpha(1.0f)
		, BlendMode(LSBM_AdditiveBlend)
	{}

	FLandscapeLayer(const FLandscapeLayer& OtherLayer) = default;

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid Guid;

	UPROPERTY()
	FName Name;

	UPROPERTY(Transient)
	bool bVisible;

	UPROPERTY()
	bool bLocked;

	UPROPERTY()
	float HeightmapAlpha;

	UPROPERTY()
	float WeightmapAlpha;

	UPROPERTY()
	TEnumAsByte<enum ELandscapeBlendMode> BlendMode;

	UPROPERTY()
	TArray<FLandscapeLayerBrush> Brushes;

	UPROPERTY()
	TMap<TObjectPtr<ULandscapeLayerInfoObject>, bool> WeightmapLayerAllocationBlend; // True -> Substractive, False -> Additive
};

UCLASS(MinimalAPI, showcategories=(Display, Movement, Collision, Lighting, LOD, Input), hidecategories=(Mobility))
class ALandscape : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscape(const FObjectInitializer& ObjectInitializer);

	//~ Begin ALandscapeProxy Interface
	LANDSCAPE_API virtual ALandscape* GetLandscapeActor() override;
	LANDSCAPE_API virtual const ALandscape* GetLandscapeActor() const override;
	//~ End ALandscapeProxy Interface

#if WITH_EDITOR
	static LANDSCAPE_API FName AffectsLandscapeActorDescProperty;

	LANDSCAPE_API bool HasAllComponent(); // determine all component is in this actor
	
	// Include Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads, 
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	// Exclude Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesNoOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads,
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	LANDSCAPE_API static void SplitHeightmap(ULandscapeComponent* Comp, ALandscapeProxy* TargetProxy = nullptr, class FMaterialUpdateContext* InOutUpdateContext = nullptr, TArray<class FComponentRecreateRenderStateContext>* InOutRecreateRenderStateContext = nullptr, bool InReregisterComponent = true);
	
	//~ Begin UObject Interface.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostRegisterAllComponents() override;
	virtual void PostActorCreated() override;
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual bool CanDeleteSelectedActor(FText& OutReason) const override;
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif // WITH_EDITOR

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	//~ End UObject Interface

	/** Computes & returns bounds containing all currently loaded landscape proxies (if any) or this landscape's bounds otherwise */
	LANDSCAPE_API FBox GetLoadedBounds() const;

	LANDSCAPE_API bool IsUpToDate() const;
	LANDSCAPE_API void TickLayers(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Landscape|Runtime")
	LANDSCAPE_API void RenderHeightmap(const FTransform& InWorldTransform, const FBox2D& InExtents, UTextureRenderTarget2D* OutRenderTarget);

	bool IsValidRenderTargetFormatHeightmap(EPixelFormat InRenderTargetFormat, bool& bOutCompressHeight);

#if WITH_EDITOR
	LANDSCAPE_API virtual bool IsNaniteEnabled() const override;
	/** Computes & returns bounds containing all landscape proxies (if any) or this landscape's bounds otherwise. Note that in non-WP worlds this will call GetLoadedBounds(). */
	LANDSCAPE_API FBox GetCompleteBounds() const;
	LANDSCAPE_API void RegisterLandscapeEdMode(ILandscapeEdModeInterface* InLandscapeEdMode) { LandscapeEdMode = InLandscapeEdMode; }
	LANDSCAPE_API void UnregisterLandscapeEdMode() { LandscapeEdMode = nullptr; }
	LANDSCAPE_API bool HasLandscapeEdMode() const { return LandscapeEdMode != nullptr; }
	LANDSCAPE_API virtual bool HasLayersContent() const override;
	LANDSCAPE_API virtual void UpdateCachedHasLayersContent(bool bInCheckComponentDataIntegrity) override;
	LANDSCAPE_API void RequestSplineLayerUpdate();
	LANDSCAPE_API void RequestLayersInitialization(bool bInRequestContentUpdate = true);
	LANDSCAPE_API void RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode InModeMask = ELandscapeLayerUpdateMode::Update_All);
	LANDSCAPE_API void RequestLayersContentUpdate(ELandscapeLayerUpdateMode InModeMask);
	LANDSCAPE_API bool ReorderLayer(int32 InStartingLayerIndex, int32 InDestinationLayerIndex);
	LANDSCAPE_API FLandscapeLayer* DuplicateLayerAndMoveBrushes(const FLandscapeLayer& InOtherLayer);
	LANDSCAPE_API int32 CreateLayer(FName InName = NAME_None);
	LANDSCAPE_API void CreateDefaultLayer();
	LANDSCAPE_API void CopyOldDataToDefaultLayer();
	LANDSCAPE_API void CopyOldDataToDefaultLayer(ALandscapeProxy* Proxy);
	LANDSCAPE_API void AddLayersToProxy(ALandscapeProxy* InProxy);
	LANDSCAPE_API FIntPoint ComputeComponentCounts() const;
	LANDSCAPE_API bool IsLayerNameUnique(const FName& InName) const;
	LANDSCAPE_API void SetLayerName(int32 InLayerIndex, const FName& InName);
	LANDSCAPE_API void SetLayerAlpha(int32 InLayerIndex, const float InAlpha, bool bInHeightmap);
	LANDSCAPE_API float GetLayerAlpha(int32 InLayerIndex, bool bInHeightmap) const;
	LANDSCAPE_API float GetClampedLayerAlpha(float InAlpha, bool bInHeightmap) const;
	LANDSCAPE_API void SetLayerVisibility(int32 InLayerIndex, bool bInVisible);
	LANDSCAPE_API void SetLayerLocked(int32 InLayerIndex, bool bLocked);
	LANDSCAPE_API uint8 GetLayerCount() const;
	LANDSCAPE_API struct FLandscapeLayer* GetLayer(int32 InLayerIndex);
	LANDSCAPE_API const struct FLandscapeLayer* GetLayer(int32 InLayerIndex) const;
	LANDSCAPE_API const struct FLandscapeLayer* GetLayer(const FGuid& InLayerGuid) const;
	LANDSCAPE_API const struct FLandscapeLayer* GetLayer(const FName& InLayerName) const;
	LANDSCAPE_API int32 GetLayerIndex(FName InLayerName) const;
	LANDSCAPE_API void ForEachLayer(TFunctionRef<void(struct FLandscapeLayer&)> Fn);
	LANDSCAPE_API void GetUsedPaintLayers(int32 InLayerIndex, TArray<ULandscapeLayerInfoObject*>& OutUsedLayerInfos) const;
	LANDSCAPE_API void GetUsedPaintLayers(const FGuid& InLayerGuid, TArray<ULandscapeLayerInfoObject*>& OutUsedLayerInfos) const;
	LANDSCAPE_API void ClearPaintLayer(int32 InLayerIndex, ULandscapeLayerInfoObject* InLayerInfo);
	LANDSCAPE_API void ClearPaintLayer(const FGuid& InLayerGuid, ULandscapeLayerInfoObject* InLayerInfo);
	LANDSCAPE_API void ClearLayer(int32 InLayerIndex, TSet<TObjectPtr<ULandscapeComponent>>* InComponents = nullptr, ELandscapeClearMode InClearMode = ELandscapeClearMode::Clear_All);
	LANDSCAPE_API void ClearLayer(const FGuid& InLayerGuid, TSet<TObjectPtr<ULandscapeComponent>>* InComponents = nullptr, ELandscapeClearMode InClearMode = ELandscapeClearMode::Clear_All, bool bMarkPackageDirty = true);
	LANDSCAPE_API void DeleteLayer(int32 InLayerIndex);
	LANDSCAPE_API void CollapseLayer(int32 InLayerIndex);
	LANDSCAPE_API void DeleteLayers();
	LANDSCAPE_API void SetEditingLayer(const FGuid& InLayerGuid = FGuid());
	LANDSCAPE_API void SetGrassUpdateEnabled(bool bInGrassUpdateEnabled);
	LANDSCAPE_API const FGuid& GetEditingLayer() const;
	LANDSCAPE_API bool IsMaxLayersReached() const;
	LANDSCAPE_API void ShowOnlySelectedLayer(int32 InLayerIndex);
	LANDSCAPE_API void ShowAllLayers();
	LANDSCAPE_API void UpdateLandscapeSplines(const FGuid& InLayerGuid = FGuid(), bool bInUpdateOnlySelected = false, bool bInForceUpdateAllCompoments = false);
	LANDSCAPE_API void SetLandscapeSplinesReservedLayer(int32 InLayerIndex);
	LANDSCAPE_API struct FLandscapeLayer* GetLandscapeSplinesReservedLayer();
	LANDSCAPE_API const struct FLandscapeLayer* GetLandscapeSplinesReservedLayer() const;
	LANDSCAPE_API bool IsEditingLayerReservedForSplines() const;

	LANDSCAPE_API bool IsLayerBlendSubstractive(int32 InLayerIndex, const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj) const;
	LANDSCAPE_API void SetLayerSubstractiveBlendStatus(int32 InLayerIndex, bool InStatus, const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj);

	LANDSCAPE_API int32 GetBrushLayer(class ALandscapeBlueprintBrushBase* InBrush) const;
	LANDSCAPE_API void AddBrushToLayer(int32 InLayerIndex, class ALandscapeBlueprintBrushBase* InBrush);
	LANDSCAPE_API void RemoveBrush(class ALandscapeBlueprintBrushBase* InBrush);
	LANDSCAPE_API void RemoveBrushFromLayer(int32 InLayerIndex, class ALandscapeBlueprintBrushBase* InBrush);
	LANDSCAPE_API void RemoveBrushFromLayer(int32 InLayerIndex, int32 InBrushIndex);
	LANDSCAPE_API int32 GetBrushIndexForLayer(int32 InLayerIndex, class ALandscapeBlueprintBrushBase* InBrush);
	LANDSCAPE_API bool ReorderLayerBrush(int32 InLayerIndex, int32 InStartingLayerBrushIndex, int32 InDestinationLayerBrushIndex);
	LANDSCAPE_API class ALandscapeBlueprintBrushBase* GetBrushForLayer(int32 InLayerIndex, int32 BrushIndex) const;
	LANDSCAPE_API TArray<class ALandscapeBlueprintBrushBase*> GetBrushesForLayer(int32 InLayerIndex) const;
	LANDSCAPE_API void OnBlueprintBrushChanged();
	LANDSCAPE_API void OnLayerInfoSplineFalloffModulationChanged(ULandscapeLayerInfoObject* InLayerInfo);
	LANDSCAPE_API void OnPreSave();

	void ReleaseLayersRenderingResource();
	void ClearDirtyData(ULandscapeComponent* InLandscapeComponent);
	
	LANDSCAPE_API void ToggleCanHaveLayersContent();
	LANDSCAPE_API void ForceUpdateLayersContent(bool bIntermediateRender = false);
	LANDSCAPE_API void ForceLayersFullUpdate();
	LANDSCAPE_API void InitializeLandscapeLayersWeightmapUsage();

	LANDSCAPE_API bool ComputeLandscapeLayerBrushInfo(FTransform& OutLandscapeTransform, FIntPoint& OutLandscapeSize, FIntPoint& OutLandscapeRenderTargetSize);
	void RequestProxyLayersWeightmapUsageUpdate();
	void UpdateProxyLayersWeightmapUsage();
	void ValidateProxyLayersWeightmapUsage() const;

	LANDSCAPE_API void SetUseGeneratedLandscapeSplineMeshesActors(bool bInEnabled);
	LANDSCAPE_API bool GetUseGeneratedLandscapeSplineMeshesActors() const;
	LANDSCAPE_API bool PrepareTextureResources(bool bInWaitForStreaming);

protected:
	FName GenerateUniqueLayerName(FName InName = NAME_None) const;

private:
	bool SupportsEditLayersLocalMerge();
	void CreateLayersRenderingResource();
	void PrepareEditLayersLocalMergeResources();
	void UpdateLayersContent(bool bInWaitForStreaming = false, bool bInSkipMonitorLandscapeEdModeChanges = false, bool bIntermediateRender = false, bool bFlushRender = false);
	void MonitorShaderCompilation();
	void MonitorLandscapeEdModeChanges();
	
	int32 RegenerateLayersHeightmaps(const FUpdateLayersContentContext& InUpdateLayersContentContext);
	int32 PerformLayersHeightmapsLocalMerge(const FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersHeightmapMergeParams& InMergeParams);
	int32 PerformLayersHeightmapsGlobalMerge(const FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersHeightmapMergeParams& InMergeParams);
	void ResolveLayersHeightmapTexture(const FTextureToComponentHelper& MapHelper, const TSet<UTexture2D*>& HeightmapsToResolve, bool bIntermediateRender, bool bFlushRender, TArray<FLandscapeEditLayerComponentReadbackResult>& InOutComponentReadbackResults);

	int32 RegenerateLayersWeightmaps(FUpdateLayersContentContext& InUpdateLayersContentContext);
	int32 PerformLayersWeightmapsLocalMerge(FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersWeightmapMergeParams& InMergeParams);
	int32 PerformLayersWeightmapsGlobalMerge(FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersWeightmapMergeParams& InMergeParams);
	void ResolveLayersWeightmapTexture(const FTextureToComponentHelper& MapHelper, const TSet<UTexture2D*>& WeightmapsToResolve, bool bIntermediateRender, bool bFlushRender, TArray<FLandscapeEditLayerComponentReadbackResult>& InOutComponentReadbackResults);

	bool ResolveLayersTexture(FTextureToComponentHelper const& MapHelper, FLandscapeEditLayerReadback* InCPUReadBack, UTexture2D* InOutputTexture, bool bIntermediateRender,	bool bFlushRender,
		TArray<FLandscapeEditLayerComponentReadbackResult>& InOutComponentReadbackResults, bool bIsWeightmap);

	static bool IsUpdateFlagEnabledForModes(ELandscapeComponentUpdateFlag InFlag, uint32 InUpdateModes);
	void UpdateForChangedHeightmaps(const TArrayView<FLandscapeEditLayerComponentReadbackResult>& InComponentReadbackResults);
	void UpdateForChangedWeightmaps(const TArrayView<FLandscapeEditLayerComponentReadbackResult>& InComponentReadbackResults);
	uint32 UpdateCollisionAndClients(const TArrayView<FLandscapeEditLayerComponentReadbackResult>& Components);
	uint32 UpdateAfterReadbackResolves(const TArrayView<FLandscapeEditLayerComponentReadbackResult>& Components);

	bool PrepareLayersTextureResources(bool bInWaitForStreaming);
	bool PrepareLayersTextureResources(const TArray<FLandscapeLayer>& InLayers, bool bInWaitForStreaming);
	bool PrepareLayersBrushResources(ERHIFeatureLevel::Type InFeatureLevel, bool bInWaitForStreaming);
	void InvalidateRVTForTextures(const TSet<UTexture2D*>& InTextures);
	void PrepareLayersHeightmapsLocalMergeRenderThreadData(const FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersHeightmapMergeParams& InMergeParams, EditLayersHeightmapLocalMerge_RenderThread::FMergeInfo& OutRenderThreadData);
	void PrepareLayersWeightmapsLocalMergeRenderThreadData(const FUpdateLayersContentContext& InUpdateLayersContentContext, const FEditLayersWeightmapMergeParams& InMergeParams, EditLayersWeightmapLocalMerge_RenderThread::FMergeInfo& OutRenderThreadData);

	void UpdateLayersMaterialInstances(const TArray<ULandscapeComponent*>& InLandscapeComponents);

	void PrepareComponentDataToExtractMaterialLayersCS(const TArray<ULandscapeComponent*>& InLandscapeComponents, const FLandscapeLayer& InLayer, int32 InCurrentWeightmapToProcessIndex, const FIntPoint& InLandscapeBase, class FLandscapeTexture2DResource* InOutTextureData,
														  TArray<struct FLandscapeLayerWeightmapExtractMaterialLayersComponentData>& OutComponentData, TMap<ULandscapeLayerInfoObject*, int32>& OutLayerInfoObjects);
	void PrepareComponentDataToPackMaterialLayersCS(int32 InCurrentWeightmapToProcessIndex, const FIntPoint& InLandscapeBase, const TArray<ULandscapeComponent*>& InAllLandscapeComponents, TArray<UTexture2D*>& InOutProcessedWeightmaps,
													TArray<FLandscapeEditLayerReadback*>& OutProcessedCPUReadBacks, TArray<struct FLandscapeLayerWeightmapPackMaterialLayersComponentData>& OutComponentData);
	void ReallocateLayersWeightmaps(FUpdateLayersContentContext& InUpdateLayersContentContext, const TArray<ULandscapeLayerInfoObject*>& InBrushRequiredAllocations);
	void InitializeLayersWeightmapResources();
	bool GenerateZeroAllocationPerComponents(const TArray<ALandscapeProxy*>& InAllLandscape, const TMap<ULandscapeLayerInfoObject*, bool>& InWeightmapLayersBlendSubstractive);

	void GenerateLayersRenderQuad(const FIntPoint& InVertexPosition, float InVertexSize, const FVector2D& InUVStart, const FVector2D& InUVSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsAtlasToNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsNonAtlasToAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsMip(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, uint8 InCurrentMip, TArray<FLandscapeLayersTriangle>& OutTriangles) const;

	void ClearLayersWeightmapTextureResource(const FString& InDebugName, FTextureRenderTargetResource* InTextureResourceToClear) const;
	void DrawHeightmapComponentsToRenderTarget(const FString& InDebugName, const TArray<ULandscapeComponent*>& InComponentsToDraw, const FIntPoint& InLandscapeBase, UTexture* InHeightmapRTRead, UTextureRenderTarget2D* InOptionalHeightmapRTRead2, UTextureRenderTarget2D* InHeightmapRTWrite, ERTDrawingType InDrawType,
											   bool InClearRTWrite, struct FLandscapeLayersHeightmapShaderParameters& InShaderParams, uint8 InMipRender = 0) const;

	void DrawWeightmapComponentsToRenderTarget(const FString& InDebugName, const TArray<FIntPoint>& InSectionBaseList, const FVector2f& InScaleBias, TArray<FVector2f>* InScaleBiasPerSection, UTexture* InWeightmapRTRead, UTextureRenderTarget2D* InOptionalWeightmapRTRead2, UTextureRenderTarget2D* InWeightmapRTWrite, ERTDrawingType InDrawType,
												bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams, uint8 InMipRender) const;

	void DrawWeightmapComponentsToRenderTarget(const FString& InDebugName, const TArray<ULandscapeComponent*>& InComponentsToDraw, const FIntPoint& InLandscapeBase, UTexture* InWeightmapRTRead, UTextureRenderTarget2D* InOptionalWeightmapRTRead2, UTextureRenderTarget2D* InWeightmapRTWrite, ERTDrawingType InDrawType,
												bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams, uint8 InMipRender) const;

	void DrawHeightmapComponentsToRenderTargetMips(const TArray<ULandscapeComponent*>& InComponentsToDraw, const FIntPoint& InLandscapeBase, UTexture* InReadHeightmap, bool InClearRTWrite, struct FLandscapeLayersHeightmapShaderParameters& InShaderParams) const;
	void DrawWeightmapComponentToRenderTargetMips(const TArray<FVector2f>& InTexturePositionsToDraw, UTexture* InReadWeightmap, bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams) const;

	void CopyTexturePS(const FString& InSourceDebugName, FTextureResource* InSourceResource, const FString& InDestDebugName, FTextureResource* InDestResource) const;

	void InitializeLayers();
	
	void PrintLayersDebugRT(const FString& InContext, UTextureRenderTarget2D* InDebugRT, uint8 InMipRender = 0, bool InOutputHeight = true, bool InOutputNormals = false) const;
	void PrintLayersDebugTextureResource(const FString& InContext, FTextureResource* InTextureResource, uint8 InMipRender = 0, bool InOutputHeight = true, bool InOutputNormals = false) const;
	void PrintLayersDebugHeightData(const FString& InContext, const TArray<FColor>& InHeightmapData, const FIntPoint& InDataSize, uint8 InMipRender, bool InOutputNormals = false) const;
	void PrintLayersDebugWeightData(const FString& InContext, const TArray<FColor>& InWeightmapData, const FIntPoint& InDataSize, uint8 InMipRender) const;

	void UpdateWeightDirtyData(ULandscapeComponent* InLandscapeComponent, UTexture2D const* InWeightmap, FColor const* InOldData, FColor const* InNewData, uint8 InChannel);
	void OnDirtyWeightmap(FTextureToComponentHelper const& MapHelper, UTexture2D const* InWeightmap, FColor const* InOldData, FColor const* InNewData);
	void UpdateHeightDirtyData(ULandscapeComponent* InLandscapeComponent, UTexture2D const* InHeightmap, FColor const* InOldData, FColor const* InNewData);
	void OnDirtyHeightmap(FTextureToComponentHelper const& MapHelper, UTexture2D const* InWeightmap, FColor const* InOldData, FColor const* InNewData);

	static bool IsTextureReady(UTexture2D* InTexture, bool bInWaitForStreaming);
	static bool IsMaterialResourceCompiled(FMaterialResource* InMaterialResource, bool bInWaitForCompilation);
#endif // WITH_EDITOR

public:

#if WITH_EDITORONLY_DATA
	/** Use Nanite to render landscape as a mesh on supported platforms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Nanite, meta = (DisplayName = "Enable Nanite"))
	bool bEnableNanite = false;

	/** Landscape actor has authority on default streaming behavior for new actors : LandscapeStreamingProxies & LandscapeSplineActors */
	UPROPERTY(EditAnywhere, Category = WorldPartition)
	bool bAreNewLandscapeActorsSpatiallyLoaded = true;

	/** If true, LandscapeStreamingProxy actors have the grid size included in their name, for backward compatibility we also check the AWorldSettings::bIncludeGridSizeInNameForPartitionedActors */
	UPROPERTY()
	bool bIncludeGridSizeInNameForLandscapeActors = false;

	UPROPERTY(EditAnywhere, Category=Landscape)
	bool bCanHaveLayersContent = false;

	/*
	 * If true, WorldPartitionLandscapeSplineMeshesBuilder is responsible of generating partitioned actors of type ALandscapeSplineMeshesActor that will contain all landscape spline/controlpoints static meshes. 
	 * Source components will be editor only and hidden in game for PIE.
	 */
	UPROPERTY()
	bool bUseGeneratedLandscapeSplineMeshesActors = false;

	DECLARE_EVENT(ALandscape, FLandscapeBlueprintBrushChangedDelegate);
	FLandscapeBlueprintBrushChangedDelegate& OnBlueprintBrushChangedDelegate() { return LandscapeBlueprintBrushChangedDelegate; }

	DECLARE_EVENT_OneParam(ALandscape, FLandscapeFullHeightmapRenderDoneDelegate, UTextureRenderTarget2D*);
	FLandscapeFullHeightmapRenderDoneDelegate& OnFullHeightmapRenderDoneDelegate() { return LandscapeFullHeightmapRenderDoneDelegate; }

	/** Target Landscape Layer for Landscape Splines */
	UPROPERTY()
	FGuid LandscapeSplinesTargetLayerGuid;
	
	/** Current Editing Landscape Layer*/
	FGuid EditingLayer;

	/** Used to temporarily disable Grass Update in Editor */
	bool bGrassUpdateEnabled;

	UPROPERTY(Transient)
	bool bEnableEditorLayersTick = true;

	UPROPERTY()
	TArray<FLandscapeLayer> LandscapeLayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> HeightmapRTList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> WeightmapRTList;

	/** List of textures that are not fully streamed in yet (updated every frame to track textures that have finished streaming in) */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)	
	TArray<TWeakObjectPtr<UTexture2D>> TrackedStreamingInTextures;

private:
	FLandscapeBlueprintBrushChangedDelegate LandscapeBlueprintBrushChangedDelegate;
	FLandscapeFullHeightmapRenderDoneDelegate LandscapeFullHeightmapRenderDoneDelegate;

	/** Components affected by landscape splines (used to partially clear Layer Reserved for Splines) */
	UPROPERTY(Transient)
	TSet<TObjectPtr<ULandscapeComponent>> LandscapeSplinesAffectedComponents;

	/** Provides information from LandscapeEdMode */
	ILandscapeEdModeInterface* LandscapeEdMode;

	/** Information provided by LandscapeEdMode */
	struct FLandscapeEdModeInfo
	{
		FLandscapeEdModeInfo();

		int32 ViewMode;
		FGuid SelectedLayer;
		TWeakObjectPtr<ULandscapeLayerInfoObject> SelectedLayerInfoObject;
		ELandscapeToolTargetType::Type ToolTarget;
	};

	FLandscapeEdModeInfo LandscapeEdModeInfo;

	UPROPERTY(Transient)
	bool bLandscapeLayersAreInitialized;
	
	UPROPERTY(Transient)
	bool bLandscapeLayersAreUsingLocalMerge;

	UPROPERTY(Transient)
	bool WasCompilingShaders;

	UPROPERTY(Transient)
	uint32 LayerContentUpdateModes;
		
	UPROPERTY(Transient)
	bool bSplineLayerUpdateRequested;

	/** Time since waiting for landscape resources to be ready (for displaying a notification to the user) */
	double WaitingForLandscapeTextureResourcesStartTime = -1.0;

	/** Time since waiting for brush resources to be ready (for displaying a notification to the user) */
	double WaitingForLandscapeBrushResourcesStartTime = -1.0;

	/** Non-stackable user notifications for landscape editor */
	TSharedPtr<FLandscapeNotification> WaitingForTexturesNotification;
	TSharedPtr<FLandscapeNotification> WaitingForBrushesNotification;
	TSharedPtr<FLandscapeNotification> InvalidShadingModelNotification;
	TSharedPtr<FLandscapeNotification> TextureBakingNotification;
	TSharedPtr<FLandscapeNotification> GrassRenderingNotification;

	// Represent all the resolved paint layer, from all layers blended together (size of the landscape x material layer count)
	class FLandscapeTexture2DArrayResource* CombinedLayersWeightmapAllMaterialLayersResource;
	
	// Represent all the resolved paint layer, from the current layer only (size of the landscape x material layer count)
	class FLandscapeTexture2DArrayResource* CurrentLayersWeightmapAllMaterialLayersResource;	
	
	// Used in extracting the material layers data from layer weightmaps (size of the landscape)
	class FLandscapeTexture2DResource* WeightmapScratchExtractLayerTextureResource;	
	
	// Used in packing the material layer data contained into CombinedLayersWeightmapAllMaterialLayersResource to be set again for each component weightmap (size of the landscape)
	class FLandscapeTexture2DResource* WeightmapScratchPackLayerTextureResource;
#endif
};

#if WITH_EDITOR
class LANDSCAPE_API FScopedSetLandscapeEditingLayer
{
public:
	FScopedSetLandscapeEditingLayer(ALandscape* InLandscape, const FGuid& InLayerGUID, TFunction<void()> InCompletionCallback = TFunction<void()>());
	~FScopedSetLandscapeEditingLayer();

private:
	TWeakObjectPtr<ALandscape> Landscape;
	FGuid PreviousLayerGUID;
	TFunction<void()> CompletionCallback;
};
#endif
