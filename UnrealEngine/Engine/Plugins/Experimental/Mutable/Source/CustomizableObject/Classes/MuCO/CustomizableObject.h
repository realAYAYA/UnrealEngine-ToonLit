// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"
#include "Input/Reply.h"
#include "Logging/LogMacros.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectClothingTypes.h"
#include "MuCO/CustomizableObjectIdentifier.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "RHIDefinitions.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "SkeletalMeshTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITORONLY_DATA
#include "PerPlatformProperties.h"
#include "PerQualityLevelProperties.h"
#endif


#include "CustomizableObject.generated.h"

class FMemoryReaderView;
class FMemoryWriter64;
class FObjectPreSaveContext;
class FObjectPreSaveRootContext;
class FText;
class IAsyncReadFileHandle;
class ITargetPlatform;
class UAnimInstance;
class UCustomizableObject;
class UEdGraph;
class UMaterialInterface;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;
struct FFrame;
template <typename FuncType> class TFunctionRef;

DECLARE_MULTICAST_DELEGATE(FPostCompileDelegate)
 
// Forward declaration of Mutable classes necessary for the interface
namespace mu
{
	class Model;
	class Parameters;
}


CUSTOMIZABLEOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogMutable, Warning, All);

USTRUCT()
struct FFParameterOptionsTags
{
	GENERATED_USTRUCT_BODY()

	/** List of tags of a Parameter Options */
	UPROPERTY(Category = "CustomizablePopulation", EditAnywhere)
	TArray<FString> Tags;
};


USTRUCT()
struct FParameterTags
{
public:

	GENERATED_USTRUCT_BODY()

	/** List of tags of a parameter */
	UPROPERTY(Category = "CustomizablePopulation", EditAnywhere)
	TArray<FString> Tags;

	/** Map of options available for a parameter can have and their tags */
	UPROPERTY(Category = "CustomizablePopulation", EditAnywhere)
	TMap<FString, FFParameterOptionsTags> ParameterOptions;
};


UENUM()
enum class ECustomizableObjectRelevancy : uint8
{
	// 
	All = 0 UMETA(DisplayName = "Relevant for client and server"),
	// 
	ClientOnly = 1 UMETA(DisplayName = "Only necessary on clients")
};


// This is used to hide Mutable SDK members in the public headers.
class FCustomizableObjectPrivateData;

USTRUCT()
struct FProfileParameterDat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ProfileName;

	UPROPERTY()
	TArray<FCustomizableObjectBoolParameterValue> BoolParameters;

	////
	UPROPERTY()
	TArray<FCustomizableObjectIntParameterValue> IntParameters;

	////
	UPROPERTY()
	TArray<FCustomizableObjectFloatParameterValue> FloatParameters;

	////
	UPROPERTY()
	TArray<FCustomizableObjectTextureParameterValue> TextureParameters;

	////
	UPROPERTY()
	TArray<FCustomizableObjectVectorParameterValue> VectorParameters;

	////
	UPROPERTY()
	TArray<FCustomizableObjectProjectorParameterValue> ProjectorParameters;
};


USTRUCT()
struct FCompilationOptions
{
	GENERATED_USTRUCT_BODY()

	// Flag to know if texture compression should be enabled
	UPROPERTY()
	bool bTextureCompression = true;

	// From 0 to 3
	UPROPERTY()
	int32 OptimizationLevel = 1;

	// If enable, multi-threaded compilation will be used when compiling this models.
	// For some memory-heavy models, this may need to be disabled, since it may multiply the
	// use of memory during compilation for each thread.
	UPROPERTY()
	bool bUseParallelCompilation = true;

	// Use the disk to store intermediate compilation data. This slows down the object compilation
	// but it may be necessary for huge objects.
	UPROPERTY()
	bool bUseDiskCompilation = false;

	// Did we have the extra bones enabled when we compiled?
	bool bExtraBoneInfluencesEnabled = false;

	// Compiling for cook
	bool bIsCooking = false;

	// Save compilation data for cook on disk 
	bool bSaveCookedDataToDisk = false;

	// This can be set for additional settings
	const class ITargetPlatform* TargetPlatform = nullptr;

	// Used to force a check to compare the guids stored in the base object compared to the children to see if any of them has changes and thus mark the object as modified.
	bool bCheckChildrenGuids = false;

	// Used to prevent the current model from being stored in the editor's streamed data and cache when the compilation finishes
	bool bDontUpdateStreamedDataAndCache = false;

	// Used to enable the use of real time morph targets.
	bool bRealTimeMorphTargetsEnabled = false;

	// Used to enable the use of clothing.
	bool bClothingEnabled = false;

	// Used to enable 16 bit bone weights
	bool b16BitBoneWeightsEnabled = false;

	// Used to reduce the number of notifications when compiling objects
	bool bSilentCompilation = true;

};


UENUM()
enum class ECustomizableObjectCompilationState : uint8
{
	//
	None,
	// 
	InProgress,
	//
	Completed,
	//
	Failed
};


class FCustomizableObjectCompilerBase
{
public:

	FCustomizableObjectCompilerBase() {};

	// Ensure virtual destruction
	virtual ~FCustomizableObjectCompilerBase() {};
	
	virtual void Compile(UCustomizableObject& Object, const FCompilationOptions& Options, bool bAsync) {};

	virtual bool Tick() { return false; }
	virtual void ForceFinishCompilation() {};

	// Return true if this object doesn't reference a parent object.
	virtual bool IsRootObject(const class UCustomizableObject* Object) const { return true; }

	/** Returns the Customizable Object that does start the CO tree */
	virtual UCustomizableObject* GetRootObject( class UCustomizableObject* Object) = 0;

	/** Provides the caller with the warning and error messages produced during compilation */
	virtual void GetCompilationMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const = 0;
	
	virtual ECustomizableObjectCompilationState GetCompilationState() const { return ECustomizableObjectCompilationState::None; }
};


// Warning! MutableCompiledDataHeader must be the first data serialized in a stream
struct MutableCompiledDataStreamHeader
{
	int32 InternalVersion;
	FGuid VersionId;

	MutableCompiledDataStreamHeader() { }
	MutableCompiledDataStreamHeader(int32 InInternalVersion, FGuid InVersionId) : InternalVersion(InInternalVersion), VersionId(InVersionId) { }

	friend FArchive& operator<<(FArchive& Ar, MutableCompiledDataStreamHeader& Header)
	{
		Ar << Header.InternalVersion;
		Ar << Header.VersionId;

		return Ar;
	}
};


USTRUCT()
struct FMutableModelImageProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelImageProperties()
		: Filter(TF_Default)
		, SRGB(0)
		, FlipGreenChannel(0)
		, LODBias(0)
		, LODGroup(TEXTUREGROUP_World)
		, AddressX(TA_Clamp)
		, AddressY(TA_Clamp)
	{}

	FMutableModelImageProperties(const FString& InTextureParameterName, TextureFilter InFilter, uint32 InSRGB, 
		uint32 InFlipGreenChannel, int32 InLODBias, TEnumAsByte<enum TextureGroup> InLODGroup,
		TEnumAsByte<enum TextureAddress> InAddressX, TEnumAsByte<enum TextureAddress> InAddressY)
		: TextureParameterName(InTextureParameterName)
		, Filter(InFilter)
		, SRGB(InSRGB)
		, FlipGreenChannel(InFlipGreenChannel)
		, LODBias(InLODBias)
		, LODGroup(InLODGroup)
		, AddressX(InAddressX)
		, AddressY(InAddressY)
	{}

	// Name in the material.
	UPROPERTY()
	FString TextureParameterName;

	UPROPERTY()
	TEnumAsByte<enum TextureFilter> Filter;

	UPROPERTY()
	uint32 SRGB : 1;

	UPROPERTY()
	uint32 FlipGreenChannel : 1;

	UPROPERTY()
	int32 LODBias;

	UPROPERTY()
	TEnumAsByte<enum TextureGroup> LODGroup;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressY;

	bool operator!=(const FMutableModelImageProperties& rhs)
	{
		return
			TextureParameterName != rhs.TextureParameterName ||
			Filter != rhs.Filter ||
			SRGB != rhs.SRGB ||
			FlipGreenChannel != rhs.FlipGreenChannel ||
			LODBias != rhs.LODBias ||
			LODGroup != rhs.LODGroup ||
			AddressX != rhs.AddressX ||
			AddressY != rhs.AddressY;
	}

	friend FArchive& operator<<(FArchive& Ar, FMutableModelImageProperties& ImageProps)
	{
		Ar << ImageProps.TextureParameterName;
		Ar << ImageProps.Filter;

		// Bitfields don't serialize automatically with FArchive
		if (Ar.IsLoading())
		{
			int32 Aux = 0;
			Ar << Aux;
			ImageProps.SRGB = Aux;

			Aux = 0;
			Ar << Aux;
			ImageProps.FlipGreenChannel = Aux;
		}
		else
		{
			int32 Aux = ImageProps.SRGB;
			Ar << Aux;

			Aux = ImageProps.FlipGreenChannel;
			Ar << Aux;
		}

		Ar << ImageProps.LODBias;
		Ar << ImageProps.LODGroup;

		Ar << ImageProps.AddressX;
		Ar << ImageProps.AddressY;

		return Ar;
	}
};


USTRUCT()
struct FMutableModelParameterValue
{
	GENERATED_USTRUCT_BODY()

	FMutableModelParameterValue() = default;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	int Value = 0;
};


USTRUCT()
struct FMutableModelParameterProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelParameterProperties() = default;
	FString Name;

	UPROPERTY()
	EMutableParameterType Type = EMutableParameterType::None;

	UPROPERTY()
	int ImageDescriptionCount = 0;

	UPROPERTY()
	TArray<FMutableModelParameterValue> PossibleValues;
};


// Represents a texture used for masking-out areas of an object from projectors
USTRUCT()
struct FMaskOutTexture
{
	GENERATED_USTRUCT_BODY()

	bool operator ==(const FMaskOutTexture& Other) const
	{ 
		return SizeX == Other.SizeX && SizeY == Other.SizeY && Data == Other.Data;
	}

	void SetTextureSize(int32 InSizeX, int32 InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		Data.SetNumUninitialized(FMath::DivideAndRoundUp(InSizeX * InSizeY, NumBitsPerDWORD)); 
	}

	int32 GetSizeX() const { return SizeX; }
	int32 GetSizeY() const { return SizeY; }

	FBitReference GetTexelReference(int32 Index)
	{
		check(Index >= 0 && Index < SizeX * SizeY);
		return FBitReference(Data[Index / NumBitsPerDWORD], 1 << (Index & (NumBitsPerDWORD - 1)));
	}

private:
	UPROPERTY()
	int32 SizeX = 0;

	UPROPERTY()
	int32 SizeY = 0;

	UPROPERTY()
	TArray<uint32> Data; // Only alpha channel of PF_R8G8B8A8 is stored as binary bits
};


USTRUCT()
struct FMorphTargetInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 LodNum = 0;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetInfo& Info)
	{
		Ar << Info.Name;
		Ar << Info.LodNum;

		return Ar;
	}
};


USTRUCT()
struct FMorphTargetVertexData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector3f PositionDelta = FVector3f::ZeroVector;

	UPROPERTY()
	FVector3f TangentZDelta = FVector3f::ZeroVector;
	
	UPROPERTY()
	int32 MorphIndex = 0;

	// Unused padding so memory footprint is the same the aligned.
	int32 Padding = 0;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetVertexData& Data)
	{
		Ar << Data.PositionDelta;
		Ar << Data.TangentZDelta;
		Ar << Data.MorphIndex;

		return Ar;
	}
};
template<> struct TCanBulkSerialize<FMorphTargetVertexData> { enum { Value = true }; };

static_assert(sizeof(FMorphTargetVertexData) == 32, "");


// A USTRUCT version of FMeshToMeshVertData in SkeletalMeshTypes.h
// We are taking advantage of the padding data to store from which asset this data comes from
// maintaining the same memory footprint than the original.
USTRUCT()
struct FCustomizableObjectMeshToMeshVertData
{
	GENERATED_USTRUCT_BODY()

	FCustomizableObjectMeshToMeshVertData() = default;

	explicit FCustomizableObjectMeshToMeshVertData(const FMeshToMeshVertData& Original)
		: Weight(Original.Weight)
	{
		for (int i = 0; i < 4; ++i)
		{
			PositionBaryCoordsAndDist[i] = Original.PositionBaryCoordsAndDist[i];
			NormalBaryCoordsAndDist[i] = Original.NormalBaryCoordsAndDist[i];
			TangentBaryCoordsAndDist[i] = Original.TangentBaryCoordsAndDist[i];
			SourceMeshVertIndices[i] = Original.SourceMeshVertIndices[i];
		}
	}
		

	explicit operator FMeshToMeshVertData() const
	{
		FMeshToMeshVertData ReturnValue;

		for (int i = 0; i < 4; ++i)
		{
			ReturnValue.PositionBaryCoordsAndDist[i] = PositionBaryCoordsAndDist[i];
			ReturnValue.NormalBaryCoordsAndDist[i] = NormalBaryCoordsAndDist[i];
			ReturnValue.TangentBaryCoordsAndDist[i] = TangentBaryCoordsAndDist[i];
			ReturnValue.SourceMeshVertIndices[i] = SourceMeshVertIndices[i];
		}
		ReturnValue.Weight = Weight;

		return ReturnValue;
	}

	
	// Barycentric coords and distance along normal for the position of the final vert
	UPROPERTY()
	float PositionBaryCoordsAndDist[4] = {0.f};

	// Barycentric coords and distance along normal for the location of the unit normal endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	UPROPERTY()
	float NormalBaryCoordsAndDist[4] = {0.f};

	// Barycentric coords and distance along normal for the location of the unit Tangent endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	UPROPERTY()
	float TangentBaryCoordsAndDist[4] = {0.f};

	// Contains the 3 indices for verts in the source mesh forming a triangle, the last element
	// is a flag to decide how the skinning works, 0xffff uses no simulation, and just normal
	// skinning, anything else uses the source mesh and the above skin data to get the final position
	UPROPERTY()
	uint16	 SourceMeshVertIndices[4] = {0u, 0u, 0u, 0u};

	UPROPERTY()
	float Weight = 0.0f;

	// Dummy for alignment (8 bytes). Originally not used.
	UPROPERTY()
	int16 SourceAssetIndex = 0;
	
	//
	UPROPERTY()
	int16 SourceAssetLodIndex = 0;

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FCustomizableObjectMeshToMeshVertData& V)
	{
		Ar << V.PositionBaryCoordsAndDist[0];
		Ar << V.PositionBaryCoordsAndDist[1];
		Ar << V.PositionBaryCoordsAndDist[2];
		Ar << V.PositionBaryCoordsAndDist[3];
		Ar << V.NormalBaryCoordsAndDist[0];
		Ar << V.NormalBaryCoordsAndDist[1];
		Ar << V.NormalBaryCoordsAndDist[2];
		Ar << V.NormalBaryCoordsAndDist[3];
		Ar << V.TangentBaryCoordsAndDist[0];
		Ar << V.TangentBaryCoordsAndDist[1];
		Ar << V.TangentBaryCoordsAndDist[2];
		Ar << V.TangentBaryCoordsAndDist[3];

		Ar << V.SourceMeshVertIndices[0];
		Ar << V.SourceMeshVertIndices[1];
		Ar << V.SourceMeshVertIndices[2];
		Ar << V.SourceMeshVertIndices[3];

		Ar << V.Weight;

		Ar << V.SourceAssetIndex;
		Ar << V.SourceAssetLodIndex;
		return Ar;
	}
};
template<> struct TCanBulkSerialize<FCustomizableObjectMeshToMeshVertData> { enum { Value = true }; };

UCLASS()
class CUSTOMIZABLEOBJECT_API UMutableMaskOutCache : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> Materials; // Maps a UMaterial's asset path to a UTexture's asset path

	UPROPERTY()
	TMap<FString, FMaskOutTexture> Textures; // Maps a UTexture's asset path to the cached mask-out texture data
};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableStreamableBlock
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint16 FileIndex = 0;

	UPROPERTY()
	uint64 Offset = 0;

	UPROPERTY()
	uint32 Size = 0;


	friend FArchive& operator<<(FArchive& Ar, FMutableStreamableBlock& Data)
	{
		Ar << Data.FileIndex;
		Ar << Data.Offset;
		Ar << Data.Size;

		return Ar;
	}
};
template<> struct TCanBulkSerialize<FMutableStreamableBlock> { enum { Value = true }; };


struct CUSTOMIZABLEOBJECT_API FMutableCachedPlatformData
{
	/** */
	TArray64<uint8> ModelData;

	/** */
	TArray64<uint8> StreamableData;
};


struct FMutableRefLODInfo
{
	float ScreenSize = 0.f;
	float LODHysteresis = 0.f;
	bool bSupportUniformlyDistributedSampling = false;
	bool bAllowCPUAccess = false;

	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODInfo& Data)
	{
		Ar << Data.ScreenSize;
		Ar << Data.LODHysteresis;
		Ar << Data.bSupportUniformlyDistributedSampling;
		Ar << Data.bAllowCPUAccess;

		return Ar;
	}
};


struct FMutableRefLODRenderData
{
	bool bIsLODOptional = false;
	bool bStreamedDataInlined = false;
	
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODRenderData& Data)
	{
		Ar << Data.bIsLODOptional;
		Ar << Data.bStreamedDataInlined;

		return Ar;
	}
};


struct FMutableRefLODData
{
	FMutableRefLODInfo LODInfo;

	FMutableRefLODRenderData RenderData;
	
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODData& Data)
	{
		Ar << Data.LODInfo;
		Ar << Data.RenderData;
		
		return Ar;
	}
};


struct FMutableRefSocket
{
	FName SocketName;
	FName BoneName;

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale;

	bool bForceAlwaysAnimated = false;
	
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSocket& Data)
	{
		Ar << Data.SocketName;
		Ar << Data.BoneName;
		Ar << Data.RelativeLocation;
		Ar << Data.RelativeRotation;
		Ar << Data.RelativeScale;

		return Ar;
	}
};


struct FMutableRefSkeletalMeshSettings
{
	bool bEnablePerPolyCollision = false;

	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshSettings& Data)
	{
		Ar << Data.bEnablePerPolyCollision;

		return Ar;
	}
};

USTRUCT()
struct FMutableRefSkeletalMeshData
{
	GENERATED_BODY()
	
	// LOD info
	TArray<FMutableRefLODData> LODData;
	
	// Sockets
	TArray<FMutableRefSocket> Sockets;

	// Bounding Box
	FBoxSphereBounds Bounds;

	// Settings
	FMutableRefSkeletalMeshSettings Settings;

	// Skeleton, must be stored in the ReferencedSkeletons too
	UPROPERTY(Transient)
	TSoftObjectPtr<USkeleton> Skeleton;
	
	// PhysicsAsset, must be stored in the PhysicsAssetMap too
	UPROPERTY(Transient)
	TSoftObjectPtr<UPhysicsAsset> PhysicsAsset;
	
	// Post Processing AnimBP, must be stored in the AnimBPAssetsMap too
	UPROPERTY(Transient) 
	TSoftClassPtr<UAnimInstance> PostProcessAnimInst;
	
	// Shadow PhysicsAsset, must be stored in the PhysicsAssetMap too
	UPROPERTY(Transient)
	TSoftObjectPtr<UPhysicsAsset> ShadowPhysicsAsset;


	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshData& Data)
	{
		Ar << Data.LODData;
		Ar << Data.Sockets;
		Ar << Data.Bounds;
		Ar << Data.Settings;


		if(Ar.IsSaving())
		{
			FString AssetPath = Data.Skeleton.ToSoftObjectPath().ToString();
			Ar << AssetPath;

			AssetPath = Data.PhysicsAsset.ToSoftObjectPath().ToString();
			Ar << AssetPath;

			AssetPath = Data.PostProcessAnimInst.ToSoftObjectPath().ToString();
			Ar << AssetPath;

			AssetPath = Data.ShadowPhysicsAsset.ToSoftObjectPath().ToString();
			Ar << AssetPath;
		
		}
		else
		{
			FString SkeletonAssetPath;
			Ar << SkeletonAssetPath;
			Data.Skeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(SkeletonAssetPath));

			FString PhysicsAssetPath;
			Ar << PhysicsAssetPath;
			Data.PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(PhysicsAssetPath));
			
			FString PostProcessAnimInstAssetPath;
			Ar << PostProcessAnimInstAssetPath;
			Data.PostProcessAnimInst = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(PostProcessAnimInstAssetPath));
			
			FString ShadowPhysicsAssetPath;
			Ar << ShadowPhysicsAssetPath;
			Data.ShadowPhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(ShadowPhysicsAssetPath));
		}

		return Ar;
	}

};

USTRUCT()
struct FMutableLODSettings
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	/** Minimum LOD to render per Platform. */
	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (DisplayName = "Minimum LOD"))
	FPerPlatformInt MinLOD;

	/** Minimum LOD to render per Quality level.*/
	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (DisplayName = "Quality Level Minimum LOD"))
	FPerQualityLevelInt MinQualityLevelLOD;

	/** Override the LOD Streaming settings from the reference skeletal meshes.*/
	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (DisplayName = "Override LOD Streaming Settings"))
	bool bOverrideLODStreamingSettings = true;

	/** Enabled: streaming LODs will trigger automatic updates to generate and discard LODs. Streaming may decrease the amount of memory used, but will stress the CPU and Streaming of resources.
	  *	Keep in mind that, even though updates may be faster depending on the amount of LODs to generate, there may be more updates to process.
	  * 
	  * Disabled: all LODs will be generated at once. It may increase the amount of memory used by the meshes and the generation may take longer, but less updates will be required. */
	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (EditCondition = "bOverrideLODStreamingSettings", DisplayName = "Enable LOD Streaming"))
	FPerPlatformBool bEnableLODStreaming = true;

	/** Limit the number of LODs to stream. A value of 0 is the same as disabling streaming of LODs.*/
	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (EditCondition = "bOverrideLODStreamingSettings"))
	FPerPlatformInt NumMaxStreamedLODs = MAX_MESH_LOD_COUNT;

#endif

	/** Number of LODs that the object root has, which can be higher than the reference mesh LOD count. It's set at the end of the compilation process */
	UPROPERTY()
	int32 NumLODsInRoot = 0;

	/** Frist LOD available, some platforms may remove lower LODs when cooking, this MinLOD represents the first LOD we can generate */
	UPROPERTY()
	int32 FirstLODAvailable = 0;

	/** Whether we should stream LODs for on the running platform */
	UPROPERTY()
	bool bLODStreamingEnabled = false;

	/** If bEnableLODStreaming is true, maximum number of LODs to stream */
	UPROPERTY()
	uint32 NumLODsToStream = 0;

};


UCLASS( config=Engine )
class CUSTOMIZABLEOBJECT_API UCustomizableObjectBulk : public UObject
{
public:
	GENERATED_BODY()

	virtual void PostLoad() override;

	/** Creates and returns an array with IAsyncReadFileHandles for each BulkData file.
	 * Used by Mutable to stream in resources when generating instances. Must be deleted by the caller. */
	TArray<IAsyncReadFileHandle*> GetAsyncReadFileHandles() const;

#if WITH_EDITOR

	//~ Begin UObject Interface
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;
	//~ End UObject Interface

	/** Compute the number of files and sizes the BulkData will be split into and fix up 
	 * HashToStreamableBlock's FileIndices and Offsets. */	
	void PrepareBulkData(UCustomizableObject* InCustomizableObject, const ITargetPlatform* TargetPlatform);

#endif
	
private:

#if WITH_EDITOR

	/** Helper to store the size of each BulkData partition. Only valid while cooking */
	TArray<uint64> BulkDataFilesSize;

	/** Helper to retrieve the BulkData from within the CookAdditionalFilesOverride */
	TObjectPtr<UCustomizableObject> CustomizableObject;

#endif

	/** BulkData file paths */
	UPROPERTY()
	TArray<FString> BulkDataFileNames;
};


UCLASS( BlueprintType, config=Engine )
class CUSTOMIZABLEOBJECT_API UCustomizableObject : public UObject
{
public:
	GENERATED_BODY()

	UCustomizableObject();

	/** All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh 
	* properties for everything that Mutable doesn't create or modify. This includes data like LOD distances, Physics
	* properties, Bounding Volumes, Skeleton, etc.
	*
	* While a CustomizableObject instance is being created for the first time, and in some situation with lots of 
	* objects this may require some seconds, the Reference Skeletal Mesh is used for the actor. This works as a better
	* solution than the alternative of not showing anything, although this can be disabled with the function
	* "SetReplaceDiscardedWithReferenceMeshEnabled" (See the c++ section).
	* 
	* For more information on this topic read the Basic Concepts at work.anticto.com.
	*/
	UPROPERTY()
	TObjectPtr<class USkeletalMesh> ReferenceSkeletalMesh_DEPRECATED;

	/** All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	* properties for everything that Mutable doesn't create or modify. This includes data like LOD distances, Physics
	* properties, Bounding Volumes, Skeleton, etc.
	*
	* While a CustomizableObject instance is being created for the first time, and in some situation with lots of
	* objects this may require some seconds, the Reference Skeletal Mesh is used for the actor. This works as a better
	* solution than the alternative of not showing anything, although this can be disabled with the function
	* "SetReplaceDiscardedWithReferenceMeshEnabled" (See the c++ section).
	*
	* For more information on this topic read the Basic Concepts at work.anticto.com.
	*/
	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray< TObjectPtr<class USkeletalMesh> > ReferenceSkeletalMeshes;

	/** All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	 * properties for everything that Mutable doesn't create or modify. This struct stores the information used from
	 * the Reference Skeletal Meshes to avoid having them loaded at all times. This includes data like LOD distances,
	 * LOD render data settings, Mesh sockets, Bounding volumes, etc.
	 */
	UPROPERTY(Transient)
	TArray<FMutableRefSkeletalMeshData> ReferenceSkeletalMeshesData;

	// Hide for now, since it is not supported yet
	//UPROPERTY(EditAnywhere, Category = CustomizableObject)
	UPROPERTY()
	TObjectPtr<class UStaticMesh> ReferenceStaticMesh;

	/** List of Materials referenced by this or any child customizable object. */
	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> ReferencedMaterials;

	/** List of Material slot names for the materials referenced by this or any child customizable object. */
	UPROPERTY()
	TArray<FName> ReferencedMaterialSlotNames;

	/** List of skeletons referenced by any of the parts of this customizable object. 
	 * The position in this array is used as skeleton ID passed to Mutable bones.
	 */
	UPROPERTY()
	TArray<TSoftObjectPtr<USkeleton>> ReferencedSkeletons;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, meta = (DisplayName = "LOD Settings"))
	FMutableLODSettings LODSettings;

	UPROPERTY(VisibleAnywhere, Category = CustomizableObject)
	TArray<FMutableModelImageProperties> ImageProperties;

	UPROPERTY(Transient)
	TArray<FMorphTargetInfo> ContributingMorphTargetsInfo;
	
	UPROPERTY(Transient)
	TArray<FMorphTargetVertexData> MorphTargetReconstructionData;

	UPROPERTY(Transient)
	TArray<FCustomizableObjectClothConfigData> ClothSharedConfigsData;	

	UPROPERTY(Transient)
	TArray<FCustomizableObjectClothingAssetData> ContributingClothingAssetsData;
	
	UPROPERTY(Transient)
	TArray<FCustomizableObjectMeshToMeshVertData> ClothMeshToMeshVertData;

#if WITH_EDITORONLY_DATA

	// Hide this property because it is not used yet.
	//UPROPERTY(EditAnywhere, Category = CustomizableObject)
	UPROPERTY()
	ECustomizableObjectRelevancy Relevancy;

	// Compilation options to use in editor and for packaging for this object.
	UPROPERTY()
	FCompilationOptions CompileOptions;

	// 
	UPROPERTY(EditAnywhere, Category = CompileOptions)
	bool bDisableTextureLayoutManagement = false;

	//
	UPROPERTY(EditAnywhere, Category = CompileOptions)
	bool bEnableRealTimeMorphTargets = false;

	//
	UPROPERTY(EditAnywhere, Category = CompileOptions)
	bool bEnableClothing = false;

	// TODO: Enable 16 bit weights 
	UPROPERTY(VisibleAnywhere, Category = CompileOptions)
	bool bEnable16BitBoneWeights = false;

	// Options when compiling this customizable object (see EMutableCompileMeshType declaration for info)
	UPROPERTY(EditAnywhere, Category = CompileOptions)
	EMutableCompileMeshType MeshCompileType = EMutableCompileMeshType::LocalAndChildren;

	// Array of elements to use with compile option CompileType = WorkingSet
	UPROPERTY(EditAnywhere, Category = CompileOptions)
	TArray<TSoftObjectPtr<UCustomizableObject>> WorkingSet;

	// Editor graph
	UPROPERTY()
	TObjectPtr<UEdGraph> Source;

	// Used to verify the derived data matches this version of the Customizable Object.
	UPROPERTY()
	FGuid VersionId;

	// Set of all the guids of all the CustomizableObjects in the compilation
	UPROPERTY()
	TSet<FGuid> CustomizableObjectGuidsInCompilation;

	/* Map to identify what CustomizableObject owns a parameter. Used to display a tooltip when hovering a parameter
	   in the Prev. instance panel */
	UPROPERTY(Transient)
	TMap<FString, FString> CustomizableObjectPathMap;

	UPROPERTY(Transient)
	TMap<FString, FCustomizableObjectIdPair> GroupNodeMap; 

	// 
	UPROPERTY()
	TArray<FProfileParameterDat>  InstancePropertiesProfiles;

#endif // WITH_EDITORONLY_DATA

	// Object parameters interface. This is used to query static data about the parameters available
	// in instances of this object.

	// Get the number of parameters available in any instance.
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int GetParameterCount() const;

	// Get the index of a parameter
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int FindParameter(const FString& Name) const;

	// Get the type of a parameter
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	EMutableParameterType GetParameterType(int32 ParamIndex) const;
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	EMutableParameterType GetParameterTypeByName(const FString& Name) const;

	// Get the name of a parameter
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	const FString& GetParameterName(int32 ParamIndex) const;

	// Get the number of description images available for a parameter
	int32 GetParameterDescriptionCount(int32 ParamIndex) const;

	// Get the number of description images available for a parameter
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int32 GetParameterDescriptionCount(const FString& ParamName) const;

	// Returns how many possible options an int parameter has
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int32 GetIntParameterNumOptions(int32 ParamIndex) const;

	// Gets the Name of the option at position K in the list of available options for the int parameter.
	// Useful to enumerate the int parameter's possible options (Ex: "Hat1", "Hat2", "Cap", "Nothing")
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	const FString& GetIntParameterAvailableOption(int32 ParamIndex, int32 K) const;

	//UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int32 FindIntParameterValue( int32 ParamIndex, const FString& Value ) const;
	//UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FString FindIntParameterValueName(int32 ParamIndex, int32 ParamValue) const;

	//
	USkeletalMesh* GetRefSkeletalMesh(int32 ComponentIndex = 0);
	
	//
	FMutableRefSkeletalMeshData* GetRefSkeletalMeshData(int32 ComponentIndex = 0);
	
	TSoftObjectPtr<UMaterialInterface> GetReferencedMaterialAssetPtr(uint32 Index);

	// Call before using Mutable's Projector testing with mask out features. It should only be loaded when needed because it can spend quite a lot of memory
	// Can cause a loading hitch
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	void LoadMaskOutCache();

	// Call after having used Mutable's Projector testing with mask out features. It should be unloaded because it can spend quite a lot of memory
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	void UnloadMaskOutCache();

private: 

	// This is information about the parameters in the model that is generated at model compile time.
	UPROPERTY(Transient)
	TArray<FMutableModelParameterProperties> ParameterProperties;

	// Map of name to index of ParameterProperties.
	// use this to lookup fast by Name
	TMap<FString, int32> ParameterPropertiesLookupTable;

	// Rebuild ParameterProperties from the current compiled model.
	void UpdateParameterPropertiesFromModel();

	// This is a manual version number for the binary blobs in this asset.
	// Increasing it invalidates all the previously compiled models.
	// Warning: If while merging code both versions have changed, take the highest+1.
	static const int32 CurrentSupportedVersion = 350;

public:

#if WITH_EDITOR
	
	// Compile the object if Automatic Compilation is enabled and the object can be compiled.
	// Automatic compilation can be enabled/disabled in the Mutable's Plugin Settings.
	bool ConditionalAutoCompile();
	
	// Add a profile that stores the values of the parameters used by the CustomInstance.
	FReply AddNewParameterProfile(FString Name, class UCustomizableObjectInstance& CustomInstance);

	// Create new GUID for this CO
	void UpdateVersionId();
	FGuid GetVersionId() const { return VersionId; }

	int GetCurrentSupportedVersion() const { return CurrentSupportedVersion; };

	// Compose folder name where the data is stored
	FString GetCompiledDataFolderPath(bool bIsEditorData) const;

	// Compose file name 
	FString GetCompiledDataFileName(bool bIsModel, const ITargetPlatform* InTargetPlatform = nullptr) const;

	/** Used to set the flag IsRoot */
	void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	/** Returns a one line description of an object for viewing in the thumbnail view of the generic browser */
	FString GetDesc() override;

	bool IsEditorOnly() const override;

	// Initialize if not set.
	void InitializeIdentifier();

	// Begin UObject interface.
	void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	void PostRename(UObject* OldOuter, const FName OldName) override;
	void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	// End UObject interface.

	// To avoid missing resources in packages we load all the resources. Loading the references added in the BeginCacheForCookedPlatformData will help us ensure they are included in the final package
	void LoadReferencedObjects();
	
	// Compile the object for a specific platform - Compile for Cook Customizable Object
	void CompileForTargetPlatform(const ITargetPlatform* TargetPlatform, bool bIsOnCookStart = false);

	// Unless we are packaging there is no need for keeping all the data generated during compilation, this information is stored in the derived data.
	void ClearCompiledData();

	// Rebuild HashToStreamableBlocks and ParameterProperties from the current compiled model.
	void UpdateCompiledDataFromModel();
	
	/** Generic Save/Load methods to write/read compiled data */
	void SaveCompiledData(FArchive& Ar, bool bSkipEditorOnlyData = false);
	void LoadCompiledData(FArchive& Ar, bool bSkipEditorOnlyData = false);

	/** Load compiled data from disk, this is used to load Editor Compilations and Cook Compilations (when using OnCookStart) */
	void LoadCompiledDataFromDisk(bool bIsEditorData = true, const ITargetPlatform* InTargetPlatform = nullptr);

	/** Cache platform data for cook */
	void CachePlatformData(const ITargetPlatform* InTargetPlatform, const TArray64<uint8>& InObjectBytes, const TArray64<uint8>& InBulkBytes);

#if WITH_EDITORONLY_DATA

	/** Map of PlatformName to CachedPlatformData. Only valid while cooking. */
	TMap<FString, FMutableCachedPlatformData> CachedPlatformsData;
	
#endif
	
	void SaveEmbeddedData(FArchive& Ar);

#endif

	// Data that may be stored in the asset itself, only in packaged builds.
	void LoadEmbeddedData(FArchive& Ar);

	void PostLoad() override;

	void Serialize(FArchive& Ar) override;

	// 
	void SerializeClothingDerivedData(FMemoryWriter64& Ar);
	void DeserializeClothingDerivedData(FMemoryReaderView& Ar);


	int32 FindState( const FString& Name ) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int32 GetStateCount() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FString GetStateName(int32 StateIndex) const;

	int32 GetStateParameterCount(int32 StateIndex) const;
	int32 GetStateParameterIndex(int32 StateIndex, int32 ParameterIndex) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	int32 GetStateParameterCount(const FString& StateName) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FString GetStateParameterName(const FString& StateName, int32 ParameterIndex) const;
	FString GetStateParameterName(int32 StateIndex, int32 ParameterIndex) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FParameterUIData GetStateUIMetadata(const FString& StateName) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FParameterUIData GetStateUIMetadataFromIndex(int32 StateIndex) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FParameterUIData GetParameterUIMetadata(const FString& ParamName) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	FParameterUIData GetParameterUIMetadataFromIndex(int32 ParamIndex) const;

	TSoftObjectPtr<USkeleton> GetReferencedSkeletonAssetPtr( uint32 Index );

	/** Stores all the parameter UI metadata information for all the dependencies of this Customizable Object */
	UPROPERTY()
	TMap<FString, FParameterUIData> ParameterUIDataMap;

	/** Stores all the state UI metadata information for all the dependencies of this Customizable Object */
	UPROPERTY()
	TMap<FString, FParameterUIData> StateUIDataMap;

	/** Stores the physics assets gathered from the SkeletalMesh nodes during compilation, to be used in mesh generation in-game */
	UPROPERTY()
	TMap<FString, TSoftObjectPtr<class UPhysicsAsset>> PhysicsAssetsMap;

	/** Stores the UAnimBlueprint assets gathered from the SkeletalMesh nodes during compilation, to be used in mesh generation in-game */
	UPROPERTY()
	TMap<FString, TSoftClassPtr<UAnimInstance>> AnimBPAssetsMap;

	/** Stores the textures that will be used to mask-out areas in projectors. The cache isn't used for rendering, but for coverage testing */
	UPROPERTY()
	TSoftObjectPtr<UMutableMaskOutCache> MaskOutCache;

	/** Map of Hash to Streaming blocks, used to stream a block of data representing a resource from the BulkData */
	UPROPERTY()
	TMap<uint64, FMutableStreamableBlock> HashToStreamableBlock;

	// Customizable Object Population data start ------------------------------------------------------
	/** Array to store the selected Population Class tags for this Customizable Object */
	UPROPERTY()
	TArray<FString> CustomizableObjectClassTags;
	
	/** Array to strore all the Population Class tags */
	UPROPERTY()
	TArray<FString> PopulationClassTags;

	/** Map of parameters available for the Customizable Object and their tags */
	UPROPERTY()
	TMap<FString, FParameterTags> CustomizableObjectParametersTags;
	// Customizable Object Population data end --------------------------------------------------------

#if WITH_EDITORONLY_DATA

	/** True if this object references a parent object. This is used basically to exclude this object
	  * from cooking. This is actually derived from the source graph object node pointing to another
	  * object or not, but it needs to be cached here because the source graph is not always available.
	  * For old objects this may be false even if they are child objects until they are resaved, but 
	  * that is the conservative case and shouldn't cause a problem. */
	UPROPERTY()
	bool bIsChildObject = false;

	FPostCompileDelegate PostCompileDelegate;

	void PostCompile();
#endif

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	class UCustomizableObjectInstance* CreateInstance();

	// Return a pointer to the BulkData subobject, only valid in packaged builds
	const UCustomizableObjectBulk* GetStreamableBulkData() const { return BulkData; }

	FCustomizableObjectPrivateData* GetPrivate() const { return PrivateData.Get(); }

	// This will always return true in a packaged game
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	bool IsCompiled() const;

	// See UCustomizableObjectSystem::LockObject()
	bool IsLocked() const;

	mu::Model* GetModel() const;

#if WITH_EDITOR
	void SetModel(mu::Model* Model);
#endif

	int32 GetNumLODs() const;

	/** Modify the provided mutable parameters so that the forced values for the given customizable object state are applied. */
	void ApplyStateForcedValuesToParameters(int32 State, mu::Parameters* Parameters);

#if WITH_EDITOR

	// UObject Interface -> Data validation
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
	// End of UObject Interface

	// UObject Interface -> Asset saving
	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;
	// End of UObject Interface

private:
	/** Flag that tells us if the validation of the asset has been triggered by the saving of it.
	 * It gets consumed by IsDataValid and it is that same function the one that resets the value of this flag.
	 */
	bool bIsValidationTriggeredBySave = false;

#endif

	/** Used to prevent GC of MaskOutCache and keep it in memory while it's needed */
	UPROPERTY(Transient)
	TObjectPtr<UMutableMaskOutCache> MaskOutCache_HardRef;

	/** Unique Identifier - used to locate Model and Streamable data on disk. Should not be modified. */
	UPROPERTY()
	FGuid Identifier;

	/** Unique identifier. Regenerated each time the object is compiled. */
	UPROPERTY()
	FGuid CompilationGuid;

public:
	FGuid GetCompilationGuid() const;

private:
	/** BulkData that stores all in-game resources used by Mutable when generating instances.
	  * Only valid in packaged builds */
	UPROPERTY()
	TObjectPtr<UCustomizableObjectBulk> BulkData;

	TSharedPtr<FCustomizableObjectPrivateData> PrivateData;
};
