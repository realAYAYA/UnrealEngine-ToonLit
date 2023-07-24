// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/VolumeTexture.h"

#include "Serialization/EditorBulkData.h"
#include "Containers/Array.h"
#include "UnrealClient.h"
#include "UObject/ObjectSaveContext.h"

#include "SparseVolumeTexture.generated.h"

namespace UE { namespace Shader	{ enum class EValueType : uint8; } }

#define SPARSE_VOLUME_TILE_RES	16

struct ENGINE_API FOpenVDBToSVTConversionResult
{
	struct FSparseVolumeAssetHeader* Header;
	TArray<uint32>* PageTable;
	TArray<uint8>* PhysicalTileDataA;
	TArray<uint8>* PhysicalTileDataB;
};

DECLARE_DELEGATE_RetVal_SevenParams(bool, FConvertOpenVDBToSparseVolumeTextureDelegate, 
	TArray<uint8>& SourceFile,
	struct FSparseVolumeRawSourcePackedData& PackedDataA,
	struct FSparseVolumeRawSourcePackedData& PackedDataB,
	FOpenVDBToSVTConversionResult* OutResult,
	bool bOverrideActiveMinMax,
	FVector ActiveMin,
	FVector ActiveMax);

ENGINE_API FConvertOpenVDBToSparseVolumeTextureDelegate& OnConvertOpenVDBToSparseVolumeTexture();

enum class ESparseVolumePackedDataFormat : uint8
{
	Unorm8 = 0,
	Float16 = 1,
	Float32 = 2,
};

// Describes how the grids of a source asset map to the components of a float4 of packed data in a SVT
struct ENGINE_API FSparseVolumeRawSourcePackedData
{
	ESparseVolumePackedDataFormat Format;
	FUintVector4 SourceGridIndex;
	FUintVector4 SourceComponentIndex; // Index [0-3] of the component in the source grid.
	bool bRemapInputForUnorm; // Maps the input from its minimum and maximum value into the [0-1] range. Clamps to [0-1] otherwise.
};

ENGINE_API FArchive& operator<<(FArchive& Ar, FSparseVolumeRawSourcePackedData& PackedData);

// The structure represent the source asset in high quality. It is used to cook the runtime data
struct ENGINE_API FSparseVolumeRawSource
{
	FSparseVolumeRawSourcePackedData PackedDataA;
	FSparseVolumeRawSourcePackedData PackedDataB;
	TArray<uint8> SourceAssetFile;

	// The current data format version for the raw source data.
	static const uint32 kVersion = 0;

	// This version can be used to convert existing source data to new version later.
	uint32 Version;

	void Serialize(FArchive& Ar);

	FSparseVolumeRawSource()
		: Version(kVersion)
	{
	}
};

struct ENGINE_API FSparseVolumeAssetHeader
{
	FIntVector3 PageTableVolumeResolution;
	FIntVector3 TileDataVolumeResolution;
	FIntVector3 SourceVolumeResolution;
	FIntVector3 SourceVolumeAABBMin;
	EPixelFormat PackedDataAFormat;
	EPixelFormat PackedDataBFormat;

	// The current data format version for the header.
	static const uint32 kVersion = 0;

	// This version can be used to convert existing header to new version later.
	uint32 Version;

	void Serialize(FArchive& Ar);

	FSparseVolumeAssetHeader()
		: PageTableVolumeResolution(FIntVector3(0, 0, 0))
		, TileDataVolumeResolution(FIntVector3(0, 0, 0))
		, SourceVolumeResolution(FIntVector3(0, 0, 0))
		, PackedDataAFormat(PF_Unknown)
		, Version(kVersion)
	{
	}
};

// The structure represent the runtime data cooked runtime data.
struct ENGINE_API FSparseVolumeTextureRuntime
{
	FSparseVolumeAssetHeader	Header;
	TArray<uint32>				PageTable;
	TArray<uint8>				PhysicalTileDataA;
	TArray<uint8>				PhysicalTileDataB;

	void SetAsDefaultTexture();

	// The current data format version for the raw source data.
	static const uint32 kVersion = 0;

	// This version can be used to convert existing source data to new version later.
	uint32 Version;

	void Serialize(FArchive& Ar);

	FSparseVolumeTextureRuntime()
		: Header()
		, Version(kVersion)
	{
	}
};


class FSparseVolumeTextureSceneProxy : public FRenderResource
{
public:

	FSparseVolumeTextureSceneProxy();
	virtual ~FSparseVolumeTextureSceneProxy() override;

	void InitialiseRuntimeData(FSparseVolumeTextureRuntime& SparseVolumeTextureRuntime);

	const FSparseVolumeAssetHeader& GetHeader() const 
	{
		check(SparseVolumeTextureRuntime);
		return SparseVolumeTextureRuntime->Header;
	}

	FTextureRHIRef GetPhysicalTileDataATextureRHI() const
	{
		return PhysicalTileDataATextureRHI;
	}
	FTextureRHIRef GetPhysicalTileDataBTextureRHI() const
	{
		return PhysicalTileDataBTextureRHI;
	}
	FTextureRHIRef GetPageTableTextureRHI() const
	{
		return PageTableTextureRHI;
	}

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

private:

	FSparseVolumeTextureRuntime*		SparseVolumeTextureRuntime;

	FTextureRHIRef						PageTableTextureRHI;
	FTextureRHIRef						PhysicalTileDataATextureRHI;
	FTextureRHIRef						PhysicalTileDataBTextureRHI;
};


struct ENGINE_API FSparseVolumeTextureFrame
{
	FSparseVolumeTextureFrame();
	virtual ~FSparseVolumeTextureFrame();
	bool BuildRuntimeData();

	// The frame data that can be streamed in when in game.
	FByteBulkData						RuntimeStreamedInData;

	// The runtime frame data that is created.
	FSparseVolumeTextureRuntime			SparseVolumeTextureRuntime;

	// The render side proxy for the sparse volume texture asset.
	FSparseVolumeTextureSceneProxy		SparseVolumeTextureSceneProxy;

#if WITH_EDITORONLY_DATA
	/** The raw data that can be loaded when we want to update cook the data with different settings or updated code without re importing. */
	UE::Serialization::FEditorBulkData	RawData;
#endif
};


enum ESparseVolumeTextureShaderUniform
{
	ESparseVolumeTexture_PhysicalUVToPageUV,
	ESparseVolumeTexture_TileSize,
	ESparseVolumeTexture_PageTableSize,
	ESparseVolumeTexture_Count,
};

// SparseVolumeTexture base interface to communicate with material graph and shader bindings.
UCLASS(ClassGroup = Rendering, BlueprintType)
class ENGINE_API USparseVolumeTexture : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	USparseVolumeTexture();
	virtual ~USparseVolumeTexture() = default;

	virtual int32 GetFrameCount() const { return 0; }
	virtual const FSparseVolumeAssetHeader* GetSparseVolumeTextureHeader(int32 FrameIndex) const { return nullptr; }
	virtual FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) { return nullptr; }
	virtual const FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) const { return nullptr; }

	/** Getter for the shader uniform parameters with index as ESparseVolumeTextureShaderUniform. */
	virtual FVector4 GetUniformParameter(int32 Index, int32 FrameIndex) const { return FVector4(ForceInitToZero); }

	virtual FBox GetVolumeBounds() const { return FBox(); }

	/** In order to keep the contents of an animated SVT sequence stable in world space, we need to account for the fact that
		different frames of the sequence have different AABBs. We solve this by scaling and biasing UVs that are relative to
		the volume bounds into the UV space represented by the AABB of each animation frame.*/
	void GetFrameUVScaleBias(int32 FrameIndex, FVector* OutScale, FVector* OutBias) const;

	/** Getter for the shader uniform parameter type with index as ESparseVolumeTextureShaderUniform. */
	static UE::Shader::EValueType GetUniformParameterType(int32 Index);

private:
};


UCLASS(ClassGroup = Rendering, BlueprintType)//, hidecategories = (Object))
class ENGINE_API UStaticSparseVolumeTexture : public USparseVolumeTexture
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "Rendering", meta = (DisplayName = "Volume Bounds"))
	FBox VolumeBounds;

	UStaticSparseVolumeTexture();
	virtual ~UStaticSparseVolumeTexture() = default;

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.


	//~ Begin USparseVolumeTexture Interface.
	int32 GetFrameCount() const override { return 1; }
	const FSparseVolumeAssetHeader* GetSparseVolumeTextureHeader(int32 FrameIndex) const override;
	FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) override;
	const FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) const override;

	/** Getter for the shader uniform parameters. */
	FVector4 GetUniformParameter(int32 Index, int32 FrameIndex) const override;
	FBox GetVolumeBounds() const override;
	//~ End USparseVolumeTexture Interface.

private:

#if WITH_EDITOR
	friend class USparseVolumeTextureFactory; // Importer
#endif

	FSparseVolumeTextureFrame StaticFrame;

	void ConvertRawSourceDataToSparseVolumeTextureRuntime();
	void GenerateOrLoadDDCRuntimeData();
	void GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy();
};

// UAnimatedSparseVolumeTexture inherit from USparseVolumeTexture to be viewed using the first frame by default.
UCLASS(ClassGroup = Rendering, BlueprintType)//, hidecategories = (Object))
class ENGINE_API UAnimatedSparseVolumeTexture : public USparseVolumeTexture
{
	GENERATED_UCLASS_BODY()

public:

	// The asset frame count.
	UPROPERTY(VisibleAnywhere, Category = "Animation", meta = (DisplayName = "Frame Count"))
	int32 FrameCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Rendering", meta = (DisplayName = "Volume Bounds"))
	FBox VolumeBounds;

	UAnimatedSparseVolumeTexture();
	virtual ~UAnimatedSparseVolumeTexture() = default;

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.


	//~ Begin USparseVolumeTexture Interface.
	int32 GetFrameCount() const override { return FrameCount; }
	const FSparseVolumeAssetHeader* GetSparseVolumeTextureHeader(int32 FrameIndex) const override;
	FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) override;
	const FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureSceneProxy(int32 FrameIndex) const override;

	/** Getter for the shader uniform parameters. */
	FVector4 GetUniformParameter(int32 Index, int32 FrameIndex) const override;
	FBox GetVolumeBounds() const override;
	//~ End USparseVolumeTexture Interface.

	// Used for debugging a specific frame of an animated sequence.
	const FSparseVolumeTextureSceneProxy* GetSparseVolumeTextureFrameSceneProxy(int32 FrameIndex) const;

private:

#if WITH_EDITOR
	friend class USparseVolumeTextureFactory; // Importer
#endif
	
	const bool bLoadAllFramesToProxies = true;	// SVT_TODO remove that once streaming is working
	TArray<FSparseVolumeTextureFrame> AnimationFrames;

	int32 GetFrameCountToLoad() const;
	void ConvertRawSourceDataToSparseVolumeTextureRuntime(int32 FrameIndex);
	void GenerateOrLoadDDCRuntimeData(int32 FrameIndex);
	void GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy(int32 FrameIndex);
};
