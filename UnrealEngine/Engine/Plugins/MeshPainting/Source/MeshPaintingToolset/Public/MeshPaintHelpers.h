// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "MeshPaintingToolsetTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IMeshPaintComponentAdapter.h"
#include "InteractiveToolManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "Engine/StaticMesh.h"
#include "Subsystems/EngineSubsystem.h"
#include "MeshPaintHelpers.generated.h"

class FMeshPaintParameters;
class UImportVertexColorOptions;
class UTexture2D;
class UStaticMeshComponent;
class USkeletalMesh;
class IMeshPaintComponentAdapter;
class UPaintBrushSettings;
class FEditorViewportClient;
class UMeshComponent;
class USkeletalMeshComponent;
class UViewportInteractor;
class FViewport;
class FPrimitiveDrawInterface;
class FSceneView;
struct FStaticMeshComponentLODInfo;
class UMeshVertexPaintingToolProperties;
class UBrushBaseProperties;

enum class EMeshPaintDataColorViewMode : uint8;

/** struct used to store the color data copied from mesh instance to mesh instance */
struct FPerLODVertexColorData
{
	TArray< FColor > ColorsByIndex;
	TMap<FVector, FColor> ColorsByPosition;
};

/** struct used to store the color data copied from mesh component to mesh component */
struct FPerComponentVertexColorData
{
	FPerComponentVertexColorData(const UStaticMesh* InStaticMesh, int32 InComponentIndex)
		: OriginalMesh(InStaticMesh)
		, ComponentIndex(InComponentIndex)
	{
	}

	/** We match up components by the mesh they use */
	TWeakObjectPtr<const UStaticMesh> OriginalMesh;

	/** We also match by component index */
	int32 ComponentIndex;

	/** Vertex colors by LOD */
	TArray<FPerLODVertexColorData> PerLODVertexColorData;
};

/** Struct to hold MeshPaint settings on a per mesh basis */
struct FInstanceTexturePaintSettings
{
	UTexture2D* SelectedTexture;
	int32 SelectedUVChannel;

	FInstanceTexturePaintSettings()
		: SelectedTexture(nullptr)
		, SelectedUVChannel(0)
	{}
	FInstanceTexturePaintSettings(UTexture2D* InSelectedTexture, int32 InSelectedUVSet)
		: SelectedTexture(InSelectedTexture)
		, SelectedUVChannel(InSelectedUVSet)
	{}

	void operator=(const FInstanceTexturePaintSettings& SrcSettings)
	{
		SelectedTexture = SrcSettings.SelectedTexture;
		SelectedUVChannel = SrcSettings.SelectedUVChannel;
	}
};


UENUM()
enum class ETexturePaintWeightTypes : uint8
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
enum class ETexturePaintWeightIndex : uint8
{
	TextureOne = 0,
	TextureTwo,
	TextureThree,
	TextureFour,
	TextureFive
};

/** Parameters for paint actions, stored together for convenience */
struct FPerVertexPaintActionArgs
{
	IMeshPaintComponentAdapter* Adapter;
	UMeshVertexPaintingToolProperties* BrushProperties;
	FVector CameraPosition;
	FHitResult HitResult;
	EMeshPaintModeAction Action;
};

/** Delegates used to call per-vertex/triangle actions */
DECLARE_DELEGATE_TwoParams(FPerVertexPaintAction, FPerVertexPaintActionArgs& /*Args*/, int32 /*VertexIndex*/);
DECLARE_DELEGATE_ThreeParams(FPerTrianglePaintAction, IMeshPaintComponentAdapter* /*Adapter*/, int32 /*TriangleIndex*/, const int32[3] /*Vertex Indices*/);

UCLASS()
class MESHPAINTINGTOOLSET_API UMeshPaintingSubsystem : public UEngineSubsystem
{

	GENERATED_BODY()
public:
	UMeshPaintingSubsystem();

	bool HasPaintableMesh(UActorComponent* Component);
	/** Removes vertex colors associated with the object */
	void RemoveInstanceVertexColors(UObject* Obj);

	/** Removes vertex colors associated with the mesh component */
	void RemoveComponentInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent);
	
	/** Propagates per-instance vertex colors to the underlying Mesh for the given LOD Index */
	bool PropagateColorsToRawMesh(UStaticMesh* StaticMesh, int32 LODIndex, FStaticMeshComponentLODInfo& ComponentLODInfo);	

	/** Retrieves the Vertex Color buffer size for the given LOD level in the Mesh */
	uint32 GetVertexColorBufferSize(UMeshComponent* MeshComponent, int32 LODIndex, bool bInstance);

	/** Retrieves the vertex positions from the given LOD level in the Mesh */
	TArray<FVector> GetVerticesForLOD(const UStaticMesh* StaticMesh, int32 LODIndex);

	/** Retrieves the vertex colors from the given LOD level in the Mesh */
	TArray<FColor> GetColorDataForLOD(const UStaticMesh* StaticMesh, int32 LODIndex);

	/** Retrieves the per-instance vertex colors from the given LOD level in the StaticMeshComponent */
	TArray<FColor> GetInstanceColorDataForLOD(const UStaticMeshComponent* MeshComponent, int32 LODIndex);

	/** Sets the specific (LOD Index) per-instance vertex colors for the given StaticMeshComponent to the supplied Color array */
	void SetInstanceColorDataForLOD(UStaticMeshComponent* MeshComponent, int32 LODIndex, const TArray<FColor>& Colors);	
	
	/** Sets the specific (LOD Index) per-instance vertex colors for the given StaticMeshComponent to a single Color value */
	void SetInstanceColorDataForLOD(UStaticMeshComponent* MeshComponent, int32 LODIndex, const FColor FillColor, const FColor MaskColor);
	
	/** Fills all vertex colors for all LODs found in the given mesh component with Fill Color */
	void FillStaticMeshVertexColors(UStaticMeshComponent* MeshComponent, int32 LODIndex, const FColor FillColor, const FColor MaskColor);
	void FillSkeletalMeshVertexColors(USkeletalMeshComponent* MeshComponent, int32 LODIndex, const FColor FillColor, const FColor MaskColor);
	
	/** Sets all vertex colors for a specific LOD level in the SkeletalMesh to FillColor */
	void SetColorDataForLOD(USkeletalMesh* SkeletalMesh, int32 LODIndex, const FColor FillColor, const FColor MaskColor);

	void ApplyFillWithMask(FColor& InOutColor, const FColor& MaskColor, const FColor& FillColor);

	/** Forces the component to render LOD level at LODIndex instead of the view-based LOD level ( X = 0 means do not force the LOD, X > 0 means force the lod to X - 1 ) */
	void ForceRenderMeshLOD(UMeshComponent* Component, int32 LODIndex);

	/** Clears all texture overrides for this component. */
	void ClearMeshTextureOverrides(const IMeshPaintComponentAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	/** Applies vertex color painting found on LOD 0 to all lower LODs. */
	void ApplyVertexColorsToAllLODs(IMeshPaintComponentAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	/** Applies the vertex colors found in LOD level 0 to all contained LOD levels in the StaticMeshComponent */
	void ApplyVertexColorsToAllLODs(IMeshPaintComponentAdapter& GeometryInfo, UStaticMeshComponent* StaticMeshComponent);

	/** Applies the vertex colors found in LOD level 0 to all contained LOD levels in the SkeletalMeshComponent */
	void ApplyVertexColorsToAllLODs(IMeshPaintComponentAdapter& GeometryInfo, USkeletalMeshComponent* SkeletalMeshComponent);

	/** Returns the number of Mesh LODs for the given MeshComponent */
	int32 GetNumberOfLODs(const UMeshComponent* MeshComponent);

	/** OutNumLODs is set to number of Mesh LODs for the given MeshComponent and returns true, or returns false of given mesh component has no valid LODs */
	bool TryGetNumberOfLODs(const UMeshComponent* MeshComponent, int32& OutNumLODs);
	
	/** Returns the number of Texture Coordinates for the given MeshComponent */
	int32 GetNumberOfUVs(const UMeshComponent* MeshComponent, int32 LODIndex);

	/** Checks whether or not the mesh components contains per lod colors (for all LODs)*/
	bool DoesMeshComponentContainPerLODColors(const UMeshComponent* MeshComponent);

	/** Retrieves the number of bytes used to store the per-instance LOD vertex color data from the mesh component */
	void GetInstanceColorDataInfo(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, int32& OutTotalInstanceVertexColorBytes);

	/** Given arguments for an action, and an action - retrieves influences vertices and applies Action to them */
	bool ApplyPerVertexPaintAction(FPerVertexPaintActionArgs& InArgs, FPerVertexPaintAction Action);

	bool GetPerVertexPaintInfluencedVertices(FPerVertexPaintActionArgs& InArgs, TSet<int32>& InfluencedVertices);

	/** Given the adapter, settings and view-information retrieves influences triangles and applies Action to them */
	bool ApplyPerTrianglePaintAction(IMeshPaintComponentAdapter* Adapter, const FVector& CameraPosition, const FVector& HitPosition, const UBrushBaseProperties* Settings, FPerTrianglePaintAction Action, bool bOnlyFrontFacingTriangles);

	/** Applies vertex painting to InOutvertexColor according to the given parameters  */
	bool PaintVertex(const FVector& InVertexPosition, const FMeshPaintParameters& InParams, FColor& InOutVertexColor);

	/** Applies Vertex Color Painting according to the given parameters */
	void ApplyVertexColorPaint(const FMeshPaintParameters &InParams, const FLinearColor &OldColor, FLinearColor &NewColor, const float PaintAmount);

	/** Applies Vertex Blend Weight Painting according to the given parameters */
	void ApplyVertexWeightPaint(const FMeshPaintParameters &InParams, const FLinearColor &OldColor, FLinearColor &NewColor, const float PaintAmount);

	/** Generate texture weight color for given number of weights and the to-paint index */
	FLinearColor GenerateColorForTextureWeight(const int32 NumWeights, const int32 WeightIndex);

	/** Computes the Paint power multiplier value */
	float ComputePaintMultiplier(float SquaredDistanceToVertex2D, float BrushStrength, float BrushInnerRadius, float BrushRadialFalloff, float BrushInnerDepth, float BrushDepthFallof, float VertexDepthToBrush);

	/** Checks whether or not a point is influenced by the painting brush according to the given parameters*/
	bool IsPointInfluencedByBrush(const FVector& InPosition, const FMeshPaintParameters& InParams, float& OutSquaredDistanceToVertex2D, float& OutVertexDepthToBrush);

	bool IsPointInfluencedByBrush(const FVector2D& BrushSpacePosition, const float BrushRadiusSquared, float& OutInRangeValue);

	template<typename T>
	void ApplyBrushToVertex(const FVector& VertexPosition, const FMatrix& InverseBrushMatrix, const float BrushRadius, const float BrushFalloffAmount, const float BrushStrength, const T& PaintValue, T& InOutValue);

	/** Helper function to retrieve vertex color from a UTexture given a UVCoordinate */
	FColor PickVertexColorFromTextureData(const uint8* MipData, const FVector2D& UVCoordinate, const UTexture2D* Texture, const FColor ColorMask);	

	void SetSelectionHasMaterialValidForTexturePaint(const bool InValidity)
	{
		bSelectionHasMaterialValidForTexturePaint = InValidity;
	}

	bool SelectionHasMaterialValidForTexturePaint()
	{
		return bSelectionHasMaterialValidForTexturePaint;
	}

	/** Map of geometry adapters for each selected mesh component */
	TSharedPtr<IMeshPaintComponentAdapter> GetAdapterForComponent(const UMeshComponent* InComponent) const;
	void AddToComponentToAdapterMap(const UMeshComponent* InComponent, const TSharedPtr<IMeshPaintComponentAdapter> InAdapter);

	TArray<UMeshComponent*> GetSelectedMeshComponents() const;
	void AddSelectedMeshComponents(const TArray<UMeshComponent*>& InComponents);
	bool FindHitResult(const FRay Ray, FHitResult& BestTraceResult);
	void ClearSelectedMeshComponents();
	TArray<UMeshComponent*> GetPaintableMeshComponents() const;
	void AddPaintableMeshComponent(UMeshComponent* InComponent);
	void ClearPaintableMeshComponents();
	bool SelectionContainsValidAdapters() const;
	TArray<FPerComponentVertexColorData> GetCopiedColorsByComponent() const;
	void SetCopiedColorsByComponent(TArray<FPerComponentVertexColorData>& InCopiedColors);
	void CacheSelectionData(const int32 PaintLODIndex, const int32 UVChannel);
	int32 GetMaxUVIndexToPaint() const;
	void ResetState();
	void Refresh();
	bool SelectionContainsPerLODColors() const { return bSelectionContainsPerLODColors; }
	void ClearSelectionLODColors() { bSelectionContainsPerLODColors = false; }

public:
	bool bNeedsRecache;

protected:
	bool bSelectionHasMaterialValidForTexturePaint;

private:
	void CleanUp();

private:
	/** Map of geometry adapters for each selected mesh component */
	TMap<FString, TSharedPtr<IMeshPaintComponentAdapter>> ComponentToAdapterMap;

	/** Currently selected mesh components as provided by the mode class */
	TArray<TWeakObjectPtr<UMeshComponent>> SelectedMeshComponents;

	/** Mesh components within the current selection which are eligible for painting */
	TArray<TWeakObjectPtr<UMeshComponent>> PaintableComponents;

	/** Contains copied vertex color data */
	TArray<FPerComponentVertexColorData> CopiedColorsByComponent;
	bool bSelectionContainsPerLODColors;
};

template<typename T>
void UMeshPaintingSubsystem::ApplyBrushToVertex(const FVector& VertexPosition, const FMatrix& InverseBrushMatrix, const float BrushRadius, const float BrushFalloffAmount, const float BrushStrength, const T& PaintValue, T& InOutValue)
{
	const FVector BrushSpacePosition = InverseBrushMatrix.TransformPosition(VertexPosition);
	const FVector2D BrushSpacePosition2D(BrushSpacePosition.X, BrushSpacePosition.Y);
		
	float InfluencedValue = 0.0f;
	if (IsPointInfluencedByBrush(BrushSpacePosition2D, BrushRadius * BrushRadius, InfluencedValue))
	{
		float InnerBrushRadius = BrushFalloffAmount * BrushRadius;
		float PaintStrength = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->ComputePaintMultiplier(BrushSpacePosition2D.SizeSquared(), BrushStrength, InnerBrushRadius, BrushRadius - InnerBrushRadius, 1.0f, 1.0f, 1.0f);

		const T OldValue = InOutValue;
		InOutValue = FMath::LerpStable(OldValue, PaintValue, PaintStrength);
	}	
};
