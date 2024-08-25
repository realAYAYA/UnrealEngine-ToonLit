// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObject.h"
#include "MuCO/StateMachine.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/CustomizableObjectIdentifier.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "MuR/Types.h"

#if WITH_EDITOR
#include "Misc/Guid.h"
#endif

#include "CustomizableObjectPrivate.generated.h"

namespace mu { class Model; }
class USkeletalMesh;
class USkeleton;
class UPhysicsAsset;
class UMaterialInterface;
class UTexture;
class UAnimInstance;
class UAssetUserData;
class UCustomizableObject;


FGuid CUSTOMIZABLEOBJECT_API GenerateIdentifier(const UCustomizableObject& CustomizableObject);


class CUSTOMIZABLEOBJECT_API FCustomizableObjectCompilerBase
{
public:

	FCustomizableObjectCompilerBase() {};

	// Ensure virtual destruction
	virtual ~FCustomizableObjectCompilerBase() {};
	
	virtual void Compile(UCustomizableObject& Object, const FCompilationOptions& Options, bool bAsync) {};

	virtual bool Tick() { return false; }
	virtual void ForceFinishCompilation() {};

	// Return true if this object doesn't reference a parent object.
	virtual bool IsRootObject(const UCustomizableObject* Object) const { return true; }

	/** Provides the caller with the warning and error messages produced during compilation */
	virtual void GetCompilationMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const = 0;
	
	virtual ECustomizableObjectCompilationState GetCompilationState() const;
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

struct FCustomizableObjectStreameableResourceId
{
	enum class EType : uint8
	{
		None                  = 0,
		AssetUserData         = 1,
		RealTimeMorphTarget   = 2,
	};

	uint32 Id   : 24;
	uint32 Type : 8;

	friend bool operator==(FCustomizableObjectStreameableResourceId A, FCustomizableObjectStreameableResourceId B)
	{
		return BitCast<uint32>(A) == BitCast<uint32>(B);
	}
};
static_assert(sizeof(FCustomizableObjectStreameableResourceId) == sizeof(uint32));

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
	TArray<FMutableModelParameterValue> PossibleValues;
};


class FMeshCache
{
public:
	USkeletalMesh* Get(const TArray<mu::FResourceID>& Key);

	void Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value);

private:
	TMap<TArray<mu::FResourceID>, TWeakObjectPtr<USkeletalMesh>> GeneratedMeshes;
};


class FSkeletonCache
{
public:
	USkeleton* Get(const TArray<uint16>& Key);

	void Add(const TArray<uint16>& Key, USkeleton* Value);

private:
	TMap<TArray<uint16>, TWeakObjectPtr<USkeleton>> MergedSkeletons;
};


struct FCustomizableObjectStatusTypes
{
	enum class EState : uint8
	{
		Loading = 0, // Waiting for PostLoad and Asset Registry to finish.
		ModelLoaded, // Model loaded correctly.
		NoModel, // No model (due to no model not found and automatic compilations disabled).
		// Compiling, // Compiling the CO.

		Count,
	};
	
	static constexpr EState StartState = EState::NoModel;

	static constexpr bool ValidTransitions[3][3] =
	{
		// TO
		// Loading, ModelLoaded, NoModel // FROM
		{false,   true,        true},  // Loading
		{false,   true,        true},  // ModelLoaded
		{true,    true,        true},  // NoModel
	};
};

using FCustomizableObjectStatus = FStateMachine<FCustomizableObjectStatusTypes>;


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


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableModelImageProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelImageProperties()
		: Filter(TF_Default)
		, SRGB(0)
		, FlipGreenChannel(0)
		, IsPassThrough(0)
		, LODBias(0)
		, LODGroup(TEXTUREGROUP_World)
		, AddressX(TA_Clamp)
		, AddressY(TA_Clamp)
	{}

	FMutableModelImageProperties(const FString& InTextureParameterName, TextureFilter InFilter, uint32 InSRGB,
		uint32 InFlipGreenChannel, uint32 bInIsPassThrough, int32 InLODBias, TEnumAsByte<enum TextureGroup> InLODGroup,
		TEnumAsByte<enum TextureAddress> InAddressX, TEnumAsByte<enum TextureAddress> InAddressY)
		: TextureParameterName(InTextureParameterName)
		, Filter(InFilter)
		, SRGB(InSRGB)
		, FlipGreenChannel(InFlipGreenChannel)
		, IsPassThrough(bInIsPassThrough)
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
	uint32 IsPassThrough : 1;

	UPROPERTY()
	int32 LODBias;

	UPROPERTY()
	TEnumAsByte<enum TextureGroup> LODGroup;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressY;

	bool operator!=(const FMutableModelImageProperties& rhs) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableModelImageProperties& ImageProps);
#endif
};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableRefSocket
{
	GENERATED_BODY()

	UPROPERTY()
	FName SocketName;
	UPROPERTY()
	FName BoneName;

	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;
	UPROPERTY()
	FVector RelativeScale = FVector::ZeroVector;

	UPROPERTY()
	bool bForceAlwaysAnimated = false;

	// When two sockets have the same name, the one with higher priority will be picked and the other discarded
	UPROPERTY()
	int32 Priority = -1;

	bool operator ==(const FMutableRefSocket& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSocket& Data);
#endif
};


USTRUCT()
struct FMutableRefLODInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float ScreenSize = 0.f;

	UPROPERTY()
	float LODHysteresis = 0.f;

	UPROPERTY()
	bool bSupportUniformlyDistributedSampling = false;

	UPROPERTY()
	bool bAllowCPUAccess = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODInfo& Data);
#endif
};


USTRUCT()
struct FMutableRefLODRenderData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsLODOptional = false;

	UPROPERTY()
	bool bStreamedDataInlined = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODRenderData& Data);
#endif
};


USTRUCT()
struct FMutableRefLODData
{
	GENERATED_BODY()

	UPROPERTY()
	FMutableRefLODInfo LODInfo;

	UPROPERTY()
	FMutableRefLODRenderData RenderData;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODData& Data);
#endif
};


USTRUCT()
struct FMutableRefSkeletalMeshSettings
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnablePerPolyCollision = false;

	UPROPERTY()
	float DefaultUVChannelDensity = 0.f;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshSettings& Data);
#endif
};


USTRUCT()
struct FMutableRefAssetUserData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UCustomizableObjectResourceDataContainer> AssetUserData;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 AssetUserDataIndex = INDEX_NONE;
#endif

};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableRefSkeletalMeshData
{
	GENERATED_BODY()

	// Reference Skeletal Mesh
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// Path to load the ReferenceSkeletalMesh
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> SoftSkeletalMesh;

	// LOD info
	UPROPERTY()
	TArray<FMutableRefLODData> LODData;

	// Sockets
	UPROPERTY()
	TArray<FMutableRefSocket> Sockets;

	// Bounding Box
	UPROPERTY()
	FBoxSphereBounds Bounds = FBoxSphereBounds(ForceInitToZero);

	// Settings
	UPROPERTY()
	FMutableRefSkeletalMeshSettings Settings;

	// Skeleton
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	// PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	// Post Processing AnimBP
	UPROPERTY()
	TSoftClassPtr<UAnimInstance> PostProcessAnimInst;

	// Shadow PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> ShadowPhysicsAsset;

	// Asset user data
	UPROPERTY()
	TArray<FMutableRefAssetUserData> AssetUserData;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshData& Data);

	void InitResources(UCustomizableObject* InOuter, const ITargetPlatform* InTargetPlatform);
#endif

};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FAnimBpOverridePhysicsAssetsInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<UAnimInstance> AnimInstanceClass;

	UPROPERTY()
	TSoftObjectPtr<UPhysicsAsset> SourceAsset;

	UPROPERTY()
	int32 PropertyIndex = -1;

	bool operator==(const FAnimBpOverridePhysicsAssetsInfo& Rhs) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FAnimBpOverridePhysicsAssetsInfo& Info);
#endif
};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableSkinWeightProfileInfo
{
	GENERATED_USTRUCT_BODY()

	FMutableSkinWeightProfileInfo() {};

	FMutableSkinWeightProfileInfo(FName InName, bool InDefaultProfile, int8 InDefaultProfileFromLODIndex) : Name(InName),
	DefaultProfile(InDefaultProfile), DefaultProfileFromLODIndex(InDefaultProfileFromLODIndex) {};

	UPROPERTY()
	FName Name;

	UPROPERTY()
	bool DefaultProfile = false;

	UPROPERTY(meta = (ClampMin = 0))
	int8 DefaultProfileFromLODIndex = 0;

	bool operator==(const FMutableSkinWeightProfileInfo& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableSkinWeightProfileInfo& Info);
#endif
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
	uint32 MorphNameIndex = 0;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetVertexData& Data)
	{
		Ar << Data.PositionDelta;
		Ar << Data.TangentZDelta;
		Ar << Data.MorphNameIndex;

		return Ar;
	}
};
template<> struct TCanBulkSerialize<FMorphTargetVertexData> { enum { Value = true }; };

// Referenced materials, skeletons, passthrough textures...
USTRUCT()
struct FModelResources
{
	GENERATED_BODY()

	/** 
	 * All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	 * properties for everything that Mutable doesn't create or modify. This struct stores the information used from
	 * the Reference Skeletal Meshes to avoid having them loaded at all times. This includes data like LOD distances,
	 * LOD render data settings, Mesh sockets, Bounding volumes, etc.
	 */
	UPROPERTY()
	TArray<FMutableRefSkeletalMeshData> ReferenceSkeletalMeshesData;

	/** Skeletons used by the compiled mu::Model. */
	UPROPERTY()
	TArray<TSoftObjectPtr<USkeleton>> Skeletons;

	/** Materials used by the compiled mu::Model */
	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/** PassThrough Textures used by the mu::Model. */
	UPROPERTY()
	TArray<TSoftObjectPtr<UTexture>> PassThroughTextures;

	/** Physics assets gathered from the SkeletalMeshes, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftObjectPtr<UPhysicsAsset>> PhysicsAssets;

	/** UAnimBlueprint assets gathered from the SkeletalMesh, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftClassPtr<UAnimInstance>> AnimBPs;

	/** */
	UPROPERTY()
	TArray<FAnimBpOverridePhysicsAssetsInfo> AnimBpOverridePhysiscAssetsInfo;

	/** Material slot names for the materials referenced by the surfaces. */
	UPROPERTY()
	TArray<FName> MaterialSlotNames;

	/** Bone names of all the bones that can possibly use the generated meshes */
	UPROPERTY()
	TArray<FName> BoneNames;

	/** Mesh sockets provided by the part skeletal meshes, to be merged in the generated meshes */
	UPROPERTY()
	TArray<FMutableRefSocket> SocketArray;

	UPROPERTY()
	TArray<FMutableSkinWeightProfileInfo> SkinWeightProfilesInfo;

	UPROPERTY()
	TArray<FMutableModelImageProperties> ImageProperties;

	/** Parameter UI metadata information for all the dependencies of this Customizable Object. */
	UPROPERTY()
	TMap<FString, FParameterUIData> ParameterUIDataMap;

	/** State UI metadata information for all the dependencies of this Customizable Object */
	UPROPERTY()
	TMap<FString, FParameterUIData> StateUIDataMap;

	UPROPERTY()
	TArray<FName> RealTimeMorphTargetNames;

	UPROPERTY()
	TArray<FMutableStreamableBlock> RealTimeMorphStreamableBlocks;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FMorphTargetVertexData> EditorOnlyMorphTargetReconstructionData;
#endif
	
	/** Map of Hash to Streaming blocks, used to stream a block of data representing a resource from the BulkData */
	UPROPERTY()
	TMap<uint64, FMutableStreamableBlock> HashToStreamableBlock;

	/** Max number of components in the compiled Model. */
	UPROPERTY()
	uint8 NumComponents = 0; 

	/** Max number of LODs in the compiled Model. */
	UPROPERTY()
	uint8 NumLODs = 0;

	/** Max number of LODs to stream. Mutable will always generate at least on LOD. */
	UPROPERTY()
	uint8 NumLODsToStream = 0;

	/** First LOD available, some platforms may remove lower LODs when cooking, this MinLOD represents the first LOD we can generate */
	UPROPERTY()
	uint8 FirstLODAvailable = 0;

};

struct CUSTOMIZABLEOBJECT_API FMutableCachedPlatformData
{
	/** */
	TArray64<uint8> ModelData;

	/** */
	TArray64<uint8> StreamableData;

	/** */
	TArray64<uint8> MorphData;
};


UCLASS()
class CUSTOMIZABLEOBJECT_API UCustomizableObjectPrivate : public UObject
{
	GENERATED_BODY()

	

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> MutableModel;

	/** Stores resources to be used by MutableModel In-Game. Cooked resources. */
	UPROPERTY()
	FModelResources ModelResources;

#if WITH_EDITORONLY_DATA
	/** 
	 * Stores resources to be used by MutableModel in the Editor. Editor resources.
	 * Editor-Only to avoid packaging assets referenced by editor compilations. 
	 */
	UPROPERTY(Transient)
	FModelResources ModelResourcesEditor;
#endif

public:
	UCustomizableObjectPrivate();
	
	void SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& GetModel();
	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> GetModel() const;

	const FModelResources& GetModelResources() const;

#if WITH_EDITORONLY_DATA
	FModelResources& GetModelResources(bool bIsCooking);
#endif

	// See UCustomizableObjectSystem::LockObject()
	bool IsLocked() const;

	/** Modify the provided mutable parameters so that the forced values for the given customizable object state are applied. */
	void ApplyStateForcedValuesToParameters(int32 State, mu::Parameters* Parameters);

	int32 FindParameter(const FString& Name) const;

	EMutableParameterType GetParameterType(int32 ParamIndex) const;

	int32 FindIntParameterValue(int32 ParamIndex, const FString& Value) const;

	FParameterUIData GetStateUIMetadataFromIndex(int32 StateIndex) const;

	FParameterUIData GetStateUIMetadata(const FString& StateName) const;

	FString GetStateName(int32 StateIndex) const;

#if WITH_EDITORONLY_DATA
	void PostCompile();
#endif

	/** Returns a pointer to the BulkData subobject, only valid in packaged builds. */
	const UCustomizableObjectBulk* GetStreamableBulkData() const;

	UCustomizableObject* GetPublic() const;

#if WITH_EDITOR
	/** Compose file name. */
	FString GetCompiledDataFileName(bool bIsModel, const ITargetPlatform* InTargetPlatform = nullptr, bool bIsDiskStreamer = false);
#endif
	
	/** Rebuild ParameterProperties from the current compiled model. */
	void UpdateParameterPropertiesFromModel(const TSharedPtr<mu::Model>& Model);

	void AddUncompiledCOWarning(const FString& AdditionalLoggingInfo);

#if WITH_EDITOR
	// Create new GUID for this CO
	void UpdateVersionId();
	
	FGuid GetVersionId() const;

	// Unless we are packaging there is no need for keeping all the data generated during compilation, this information is stored in the derived data.
	void ClearCompiledData(bool bIsCooking);

	void SaveEmbeddedData(FArchive& Ar);

	// Compile the object for a specific platform - Compile for Cook Customizable Object
	void CompileForTargetPlatform(const ITargetPlatform* TargetPlatform);
	
	// Add a profile that stores the values of the parameters used by the CustomInstance.
	FReply AddNewParameterProfile(FString Name, class UCustomizableObjectInstance& CustomInstance);

	// Compose folder name where the data is stored
	FString GetCompiledDataFolderPath() const;

	/** Generic Save/Load methods to write/read compiled data */
	void SaveCompiledData(FArchive& Ar, bool bSkipEditorOnlyData = false);
	void LoadCompiledData(FArchive& Ar, const ITargetPlatform* InTargetPlatform, bool bSkipEditorOnlyData = false);

	/** Load compiled data for the running platform from disk, this is used to load Editor Compilations. */
	void LoadCompiledDataFromDisk();

	/** Cache platform data for cook */
	void CachePlatformData(const ITargetPlatform* InTargetPlatform, TArray64<uint8>& InObjectBytes, TArray64<uint8>& InBulkBytes, TArray64<uint8>& InMorphBytes);
	
	/** Loads data previously compiled in BeginCacheForCookedPlatformData onto the UProperties in *this,
	  * in preparation for saving the cooked package for *this or for a CustomizableObjectInstance using *this.
      * Returns whether the data was successfully loaded. */
	bool TryLoadCompiledCookDataForPlatform(const ITargetPlatform* TargetPlatform);
#endif

	// Data that may be stored in the asset itself, only in packaged builds.
	void LoadEmbeddedData(FArchive& Ar);
	
	/** Compute bIsChildObject if currently possible to do so. Return whether it was computed. */
	bool TryUpdateIsChildObject();

	void SetIsChildObject(bool bIsChildObject);

	/** Return the names used by mutable to identify which mu::Image should be considered of LowPriority. */
	void GetLowPriorityTextureNames(TArray<FString>& OutTextureNames);

	/** Return the MinLOD index to generate based on the active LODSettings (PerPlatformMinLOD or PerQualityLevelMinLOD) */
	int32 GetMinLODIndex() const;
	
#if WITH_EDITOR
	/** See ICustomizableObjectEditorModule::IsCompilationOutOfDate. */
	bool IsCompilationOutOfDate(TArray<FName>* OutOfDatePackages = nullptr) const;

	void OnParticipatingObjectDirty(UPackage* Package, bool);
#endif

	TArray<FString>& GetCustomizableObjectClassTags();
	
	TArray<FString>& GetPopulationClassTags();

    TMap<FString, FParameterTags>& GetCustomizableObjectParametersTags();

#if WITH_EDITORONLY_DATA
	TArray<FProfileParameterDat>& GetInstancePropertiesProfiles();
#endif
	
	TArray<FCustomizableObjectResourceData>& GetAlwaysLoadedExtensionData();

	TArray<FCustomizableObjectStreamedResourceData>& GetStreamedExtensionData();
	
	TArray<FCustomizableObjectStreamedResourceData>& GetStreamedResourceData();
	
	/** Cache of generated SkeletalMeshes */
	FMeshCache MeshCache;

	/** Cache of merged Skeletons */
	FSkeletonCache SkeletonCache;

	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITORONLY_DATA

	/** Unique Identifier - Deterministic. Used to locate Model and Streamable data on disk. Should not be modified. */
	FGuid Identifier;

	/** Cooked platform names. */
	TArray<FString> CachedPlatformNames;

	/** List of external packages that if changed, a compilation is required.
	 * Key is the package name. Value is the the UPackage::Guid, which is regenerated each time the packages is saved.
	 *
	 * Updated each time the CO is compiled and saved in the Derived Data. */
	TMap<FName, FGuid> ParticipatingObjects;

	/** List of Participating Objects (packages) has been marked as dirty since the last compilation. */
	TArray<FName> DirtyParticipatingObjects;
	
	/** Map to identify what CustomizableObject owns a parameter. Used to display a tooltip when hovering a parameter
	 * in the Prev. instance panel */
	UPROPERTY(Transient)
	TMap<FString, FString> CustomizableObjectPathMap;

	UPROPERTY(Transient)
	TMap<FString, FCustomizableObjectIdPair> GroupNodeMap;

	/** If the object is compiled, this flag is false unless it was compiled with maximum optimizations. If the object is not compiled, its value is meaningless. */
	bool bIsCompiledWithoutOptimization = true;

	/** This is a non-user-controlled flag to disable streaming (set at object compilation time, depending on optimization). */
	bool bDisableTextureStreaming = false;
	
	ECustomizableObjectCompilationState CompilationState = ECustomizableObjectCompilationState::None;
	
	FPostCompileDelegate PostCompileDelegate;

#if WITH_EDITOR
	/** Map of PlatformName to CachedPlatformData. Only valid while cooking. */
	TMap<FString, FMutableCachedPlatformData> CachedPlatformsData;
#endif
#endif

	FCustomizableObjectStatus Status;

	// This is information about the parameters in the model that is generated at model compile time.
	UPROPERTY(Transient)
	TArray<FMutableModelParameterProperties> ParameterProperties;

	// Map of name to index of ParameterProperties.
	// use this to lookup fast by Name
	TMap<FString, int32> ParameterPropertiesLookupTable;

	// This is a manual version number for the binary blobs in this asset.
	// Increasing it invalidates all the previously compiled models.
	// Warning: If while merging code both versions have changed, take the highest+1.
	static constexpr int32 CurrentSupportedVersion = 436;
};

