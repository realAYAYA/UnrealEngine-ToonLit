// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MeshPaintingToolsetTypes.generated.h"

class UMaterialInterface;
class UMeshComponent;
class UTexture2D;
class UTexture;
class UTextureRenderTarget2D;

class FMeshPaintParameters;
DECLARE_MULTICAST_DELEGATE_FourParams(FApplyVertexPaintData, const FMeshPaintParameters& /* &InParams*/, const FLinearColor& /*OldColor*/, FLinearColor& /*NewColor*/, const float /*PaintAmount*/);

/** Mesh paint color view modes (somewhat maps to EVertexColorViewMode engine enum.) */
UENUM()
enum class EMeshPaintDataColorViewMode : uint8
{
	/** Normal view mode (vertex color visualization off) */
	Normal UMETA(DisplayName = "Off"),

	/** RGB only */
	RGB UMETA(DisplayName = "RGB Channels"),

	/** Alpha only */
	Alpha UMETA(DisplayName = "Alpha Channel"),

	/** Red only */
	Red UMETA(DisplayName = "Red Channel"),

	/** Green only */
	Green UMETA(DisplayName = "Green Channel"),

	/** Blue only */
	Blue UMETA(DisplayName = "Blue Channel"),
};

/** Mesh painting action (paint, erase) */
enum class EMeshPaintModeAction : uint8
{
	/** Paint (add color or increase blending weight) */
	Paint,

	/** Erase (remove color or decrease blending weight) */
	Erase
};

/** Vertex paint target */
UENUM()
enum class EMeshVertexPaintModeTarget : uint8
{
	/** Paint the static mesh component instance in the level */
	ComponentInstance,

	/** Paint the actual static mesh asset */
	Mesh
};

/** Mesh paint parameters */
class FMeshPaintParameters
{
public:
	EMeshPaintModeAction PaintAction;
	FVector BrushPosition;
	FVector BrushNormal;
	FLinearColor BrushColor;
	float SquaredBrushRadius;
	float BrushRadialFalloffRange;
	float InnerBrushRadius;
	float BrushDepth;
	float BrushDepthFalloffRange;
	float InnerBrushDepth;
	float BrushStrength;
	FMatrix BrushToWorldMatrix;
	FMatrix InverseBrushToWorldMatrix;
	bool bWriteRed;
	bool bWriteGreen;
	bool bWriteBlue;
	bool bWriteAlpha;
	int32 TotalWeightCount;
	int32 PaintWeightIndex;
	int32 UVChannel;
	FApplyVertexPaintData ApplyVertexDataDelegate;
	FVector2f BrushPosition2D;
	bool bUseFillBucket = false;
};

/** Structure used to hold per-triangle data for texture painting */
struct FTexturePaintTriangleInfo
{
	FVector TriVertices[3];
	FVector2D TrianglePoints[3];
	FVector2D TriUVs[3];
};

/** Structure used to house and compare Texture and UV channel pairs */
struct FPaintableTexture
{
	UTexture*	Texture;
	int32		UVChannelIndex;

	FPaintableTexture()
		: Texture(nullptr)
		, UVChannelIndex(0)
	{}

	template<typename T, decltype(ImplicitConv<UTexture*>(DeclVal<T>()))* = nullptr>
	FPaintableTexture(T InTexture = nullptr, uint32 InUVChannelIndex = 0)
		: Texture(InTexture)
		, UVChannelIndex(InUVChannelIndex)
	{}

	/** Overloaded equality operator for use with TArrays Contains method. */
	bool operator==(const FPaintableTexture& rhs) const
	{
		return (Texture == rhs.Texture);
		/* && (UVChannelIndex == rhs.UVChannelIndex);*/// if we compare UVChannel we would have to duplicate the texture
	}
};

USTRUCT()
struct FPaintTexture2DData
{
	GENERATED_BODY()
	/** The original texture that we're painting */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PaintingTexture2D = nullptr;

	UPROPERTY(Transient)
	bool bIsPaintingTexture2DModified = false;

	/** Texture used to store the paint modifications for the transactions */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ScratchTexture = nullptr;

	/** Render target texture for painting */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTargetTexture = nullptr;

	/** List of component we are painting on */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PaintedComponents;
	
	/** Optional render target texture used as an input while painting that contains a clone of the texture painting brush */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintBrushRenderTargetTexture = nullptr;

	FPaintTexture2DData() = default;

	FPaintTexture2DData(UTexture2D* InPaintingTexture2D, bool InbIsPaintingTexture2DModified = false)
		: PaintingTexture2D(InPaintingTexture2D)
		, bIsPaintingTexture2DModified(InbIsPaintingTexture2DModified)
	{
	}

};

USTRUCT()
struct FPaintComponentOverride
{
	GENERATED_BODY()

	/** List of components overridden */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PaintedComponents;
};

namespace MeshPaintDefs
{
	// Design constraints

	// Currently we never support more than five channels (R, G, B, A, OneMinusTotal)
	static const int32 MaxSupportedPhysicalWeights = 4;
	static const int32 MaxSupportedWeights = MaxSupportedPhysicalWeights + 1;
}

/**
*  Wrapper to expose texture targets to WPF code.
*/
struct FTextureTargetListInfo
{
	UTexture2D* TextureData;
	bool bIsSelected;
	uint32 UndoCount;
	uint32 UVChannelIndex;
	FTextureTargetListInfo(UTexture2D* InTextureData, int32 InUVChannelIndex, bool InbIsSelected = false)
		: TextureData(InTextureData)
		, bIsSelected(InbIsSelected)
		, UndoCount(0)
		, UVChannelIndex(InUVChannelIndex)
	{}
};

/**
*  Wrapper to store which of a meshes materials is selected as well as the total number of materials.
*/
struct FMeshSelectedMaterialInfo
{
	int32 NumMaterials;
	int32 SelectedMaterialIndex;

	FMeshSelectedMaterialInfo(int32 InNumMaterials)
		: NumMaterials(InNumMaterials)
		, SelectedMaterialIndex(0)
	{}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#endif
