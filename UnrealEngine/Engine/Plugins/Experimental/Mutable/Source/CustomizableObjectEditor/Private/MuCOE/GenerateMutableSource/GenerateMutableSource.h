// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/Skeleton.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"
#include "Math/NumericLimits.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectClothingTypes.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuR/Mesh.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeScalarParameter.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/Table.h"
#include "RHIDefinitions.h"
#include "ReferenceSkeleton.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealNames.h"

class FCustomizableObjectCompiler;
class UAnimInstance;
class UCustomizableObjectNodeMaterial;
class UCustomizableObjectNodeMeshMorph;
class UCustomizableObjectNodeObjectGroup;
class UEdGraphNode;
class UMaterialInterface;
class UObject;
class UPhysicsAsset;
class UTexture2D;
struct FCustomizableObjectIdPair;
struct FMutableGraphGenerationContext;
struct FParameterUIData;


struct FGeneratedImageProperties
{
	/** Name in the Material. */
	FString TextureParameterName;

	TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_Default;

	TEnumAsByte<TextureFilter> Filter = TF_Bilinear;

	uint32 SRGB = 0;

	uint32 bFlipGreenChannel = 0;

	int32 LODBias = 0;

	TEnumAsByte<TextureMipGenSettings> MipGenSettings = TMGS_SimpleAverage;

	int32 MaxTextureSize = 0;

	TEnumAsByte<TextureGroup> LODGroup = TEnumAsByte<TextureGroup>(TMGS_FromTextureGroup);

	TEnumAsByte<TextureAddress> AddressX = TA_Clamp;
	TEnumAsByte<TextureAddress> AddressY = TA_Clamp;
};


// Flags that can influence the mesh conversion
enum class EMutableMeshConversionFlags : uint32
{
	// 
	None = 0,
	// Ignore the skeleton and skinning
	IgnoreSkinning = 1 << 0,

	// Ignore Physics assets
	IgnorePhysics = 1 << 1
};


// Key for the data stored for each processed unreal graph node.
class FGeneratedKey
{
	friend uint32 GetTypeHash(const FGeneratedKey& Key);

public:
	FGeneratedKey(void* InFunctionAddress, const UEdGraphPin& InPin, const UCustomizableObjectNode& Node, FMutableGraphGenerationContext& GenerationContext, const bool UseMesh = false);
	
	bool operator==(const FGeneratedKey& Other) const;

private:
	/** Used to differentiate pins being cached from different functions (e.g. a PC_Color pin cached from GenerateMutableSourceImage and GenerateMutableSourceColor). */
	void* FunctionAddress;
	
	const UEdGraphPin* Pin;
	
	int32 LOD;

	/** Flag used to generate this mesh. Bit mask of EMutableMeshConversionFlags */
	uint32 Flags = 0;

	/** Active morphs at the time of mesh generation. */
	TArray<UCustomizableObjectNodeMeshMorph*> MeshMorphStack;
};


uint32 GetTypeHash(const FGeneratedKey& Key);


struct FGeneratedImageKey
{
	FGeneratedImageKey(const UEdGraphPin* InPin)
	{
		Pin = InPin;
	}

	bool operator==(const FGeneratedImageKey& Other) const
	{
		return Pin == Other.Pin;
	}

	const UEdGraphPin* Pin;
};

/** Structure used to store the results of the recursive GenerateMutableSourceSurface calls. */
struct FMutableGraphSurfaceGenerationData
{
	/** Pointer to the NodeMaterial which a pin is connected (directly or indirectly). Used to find the real NodeMaterial on the NodeCopyMaterial case. 
	 * The NodeMaterial may be behind a export/import nodes, or already generated
	 */
	const UCustomizableObjectNodeMaterial* NodeMaterial = nullptr;
	
	FGeneratedImageProperties ImageProperties;
};

// Structure storing results to propagate up when generating mutable mesh node expressions.
struct FMutableGraphMeshGenerationData
{
	// Did we find any mesh with vertex colours in the expression?
	bool bHasVertexColors = false;

	// Maximum number of texture channels found in the expression.
	int NumTexCoordChannels = 0;

	// Maximum number of bones per vertex found in the expression.
	int MaxNumBonesPerVertex = 0;

	// Maximum size of the vertex bone index type in the expression.
	int MaxBoneIndexTypeSizeBytes = 0;

	int32 MaxNumTriangles = 0;
	int32 MinNumTriangles = TNumericLimits<int32>::Max();

	// Combine another generated data looking for the most general case.
	void Combine(const FMutableGraphMeshGenerationData& other)
	{
		bHasVertexColors = bHasVertexColors || other.bHasVertexColors;
		NumTexCoordChannels = FMath::Max(other.NumTexCoordChannels, NumTexCoordChannels);
		MaxNumBonesPerVertex = FMath::Max(other.MaxNumBonesPerVertex, MaxNumBonesPerVertex);
		MaxBoneIndexTypeSizeBytes = FMath::Max(other.MaxBoneIndexTypeSizeBytes, MaxBoneIndexTypeSizeBytes);
		MaxNumTriangles = FMath::Max(other.MaxNumTriangles, MaxNumTriangles);
		MinNumTriangles = FMath::Min(other.MinNumTriangles, MinNumTriangles);
	}
};


// Data stored for each processed unreal graph node, stored in the cache.
struct FGeneratedData
{
	FGeneratedData(const UEdGraphNode* InSource, mu::NodePtr InNode,
		const FMutableGraphMeshGenerationData* InMeshData = nullptr)
	{
		Source = InSource;
		Node = InNode;
		if (InMeshData)
		{
			meshData = *InMeshData;
		}
	}

	const UEdGraphNode* Source;
	mu::NodePtr Node;

	// Used for mesh nodes only
	FMutableGraphMeshGenerationData meshData;
};


inline uint32 GetTypeHash(const FGeneratedImageKey& Key)
{
	uint32 GuidHash = GetTypeHash(Key.Pin->PinId);

	return GuidHash;
}


/** Struct describing the conversion task for each Unreal texture that needs to be converted to Mutable format */
struct FTextureUnrealToMutableTask
{
	/** Default constructor */
	FTextureUnrealToMutableTask(mu::NodeImageConstantPtr ImageNodeParameter, UTexture2D* TextureParameter, const UCustomizableObjectNode* NodeParameter, bool bIsNormalCompositeParameter = false)
		: ImageNode(ImageNodeParameter)
		, Texture(TextureParameter)
		, Node(NodeParameter)
		, bIsNormalComposite(bIsNormalCompositeParameter)
	{}

	/** Table Node constructor */
	FTextureUnrealToMutableTask(mu::TablePtr TableParameter, UTexture2D* TextureParameter, const UCustomizableObjectNode* NodeParameter, int32 ColumnIndex, int32 RowIndex, bool bIsNormalCompositeParameter = false)
		: TableNode(TableParameter)
		, Texture(TextureParameter)
		, Node(NodeParameter)
		, TableColumn(ColumnIndex)
		, TableRow(RowIndex)
		, bIsNormalComposite(bIsNormalCompositeParameter)
	{}

	mu::NodeImageConstantPtr ImageNode;
	mu::TablePtr TableNode;
	UTexture2D* Texture;
	const UCustomizableObjectNode* Node;
	int32 TableColumn;
	int32 TableRow;
	bool bIsNormalComposite;	
};


struct FPoseBoneData
{
	TArray<FString> ArrayBoneName;
	TArray<FTransform> ArrayTransform;
};


struct FGroupProjectorTempData
{
	class UCustomizableObjectNodeGroupProjectorParameter* CustomizableObjectNodeGroupProjectorParameter;
	mu::NodeProjectorParameterPtr NodeProjectorParameterPtr;
	mu::NodeImagePtr NodeImagePtr;
	mu::NodeRangePtr NodeRange;
	mu::NodeScalarParameterPtr NodeOpacityParameter;

	mu::NodeScalarEnumParameterPtr PoseOptionsParameter;
	TArray<FPoseBoneData> PoseBoneDataArray;

	bool bAlternateResStateNameWarningDisplayed = false; // Used to display this warning only once
};


struct FGroupNodeIdsTempData
{
	FGroupNodeIdsTempData(FGuid OldGuid, FGuid NewGuid = FGuid()) :
		OldGroupNodeId(OldGuid),
		NewGroupNodeId(NewGuid)
	{

	}

	FGuid OldGroupNodeId;
	FGuid NewGroupNodeId;

	bool operator==(const FGroupNodeIdsTempData& Other) const
	{
		return OldGroupNodeId == Other.OldGroupNodeId;
	}
};

struct FGroupProjectorImageInfo
{
	mu::NodeImagePtr ImageNode;
	mu::NodeImagePtr ImageResizeNode;
	mu::NodeSurfaceNewPtr SurfNode;
	UCustomizableObjectNodeMaterial* TypedNodeMat;
	FString TextureName;
	FString RealTextureName;
	FString AlternateResStateName;
	float AlternateProjectionResolutionFactor;
	bool bIsAlternateResolutionResized = false;
	int32 UVLayout = 0;

	FGroupProjectorImageInfo(mu::NodeImagePtr InImageNode, const FString& InTextureName, const FString& InRealTextureName, UCustomizableObjectNodeMaterial* InTypedNodeMat,
		float InAlternateProjectionResolutionFactor, const FString& InAlternateResStateName, mu::NodeSurfaceNewPtr InSurfNode, int32 InUVLayout)
		: TypedNodeMat(InTypedNodeMat), TextureName(InTextureName), RealTextureName(InRealTextureName),
		AlternateResStateName(InAlternateResStateName), AlternateProjectionResolutionFactor(InAlternateProjectionResolutionFactor), 
		UVLayout(InUVLayout)
	{
		ImageNode = InImageNode;
		SurfNode = InSurfNode;
	}

	static FString GenerateId(const UCustomizableObjectNodeMaterialBase* TypedNodeMat, int32 ImageIndex)
	{
		return TypedNodeMat->GetOutermost()->GetPathName() + TypedNodeMat->NodeGuid.ToString() + FString("-") + FString::FromInt(ImageIndex);
	}
};


/** Struct that defines a mesh (Mesh + LOD + MaterialIndex) */
struct FMeshData
{
	/** Mesh which it may contain multiple LODs and Materials. */
	const UObject* Mesh;

	/** Specific LOD of the mesh. */
	int LOD;

	/** Specific index of the mesh material (MaterialIndex is an alias of Section). */
	int MaterialIndex;

	/** Node where the mesh is defined. Not a UCustomizableObjectNodeMesh due to Table nodes */
	const UCustomizableObjectNode* Node;

	bool operator==(const FMeshData& Other) const
	{
		return Mesh == Other.Mesh &&
			LOD == Other.LOD &&
			MaterialIndex == Other.MaterialIndex &&
			Node == Other.Node;
	}
};

inline uint32 GetTypeHash(const FMeshData& Key)
{
	uint32 MeshHash = GetTypeHash(Key.Mesh);
	uint32 NodeHash = GetTypeHash(Key.Node->GetUniqueID());
	
	return HashCombine(HashCombine(HashCombine(MeshHash, Key.LOD), Key.MaterialIndex), NodeHash);
}


/** Struct used to store info specific to each component during compilation */
struct FMutableComponentInfo
{
	FMutableComponentInfo(USkeletalMesh* InRefSkeletalMesh)
	{
		if(!InRefSkeletalMesh || !InRefSkeletalMesh->GetSkeleton())
		{
			return;
		}
		
		RefSkeletalMesh = InRefSkeletalMesh;
		RefSkeleton = RefSkeletalMesh->GetSkeleton();
			
		const int32 NumBones = RefSkeleton->GetReferenceSkeleton().GetRawBoneNum();
		BoneNamesToPathHash.Reserve(NumBones);

		const TArray<FMeshBoneInfo>& Bones = RefSkeleton->GetReferenceSkeleton().GetRawRefBoneInfo();

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FMeshBoneInfo& Bone = Bones[BoneIndex];

			// Retrieve parent bone name and respective hash, root-bone is assumed to have a parent hash of 0
			const FName ParentName = Bone.ParentIndex != INDEX_NONE ? Bones[Bone.ParentIndex].Name : NAME_None;
			const uint32 ParentHash = Bone.ParentIndex != INDEX_NONE ? GetTypeHash(ParentName) : 0;

			// Look-up the path-hash from root to the parent bone
			const uint32* ParentPath = BoneNamesToPathHash.Find(ParentName);
			const uint32 ParentPathHash = ParentPath ? *ParentPath : 0;

			// Append parent hash to path to give full path hash to current bone
			const uint32 BonePathHash = HashCombine(ParentPathHash, ParentHash);

			// Add path hash to current bone
			BoneNamesToPathHash.Add(Bone.Name, BonePathHash);
		}
	}

	// Each component must have a reference SkeletalMesh with a valid Skeleton
	USkeletalMesh* RefSkeletalMesh = nullptr;
	USkeleton* RefSkeleton = nullptr;

	// Map to check skeleton compatibility
	TMap<USkeleton*, bool> SkeletonCompatibility;
	
	// Hierarchy hash from parent-bone to root bone, used to check if additional skeletons are compatible with
	// the RefSkeleton
	TMap<FName, uint32> BoneNamesToPathHash;
};


/** Struct of data which is behind a given output pin. 
 *
 * Each newly added field has should also be defined on the Append() function body.
 */
struct FPinDataValue
{
	/** Set of all meshes behind a given output pin. */
	TSet<FMeshData> MeshesData;

	/** Add all data from other PinData. */
	void Append(const FPinDataValue& From);
};

/** Eases the managment and access of data behind a given pin.
 * 
 * We understand as "behind a pin" as all pins which are connected to a given pin directly and indirectly. 
 * 
 * Given the the following pins:
 * C -> B -> A
 *      D ---^
 * 
 * FPinData will propagate the added data in the following way:
 * C (data added on C) -> B (data added on C, B) -> A (data added on C, B, A, D)
 *                           D (data added on D) ---^
 * 
 * FPinDataValue defines which data is going to be saved behind pins.
 *
 * When traversing the graph:
 * - Each time an output pin is explored recursively, call the Push() function.
 * - When exiting the recursion, call the Pop() function.
 * - Add data to be seen behind all the stacked pins using the GetCurrent() function.
 * - To avoid missing any returning branch, use the SCOPE_PIN_DATA wrapper.
 * 
 * The all data from a behind a pin can be queried using the Find() method.
 */
class FPinData // DEPRECATED. DO NOT USE!
{
public:
	/** Get the PinData from a given pin. */
	FPinDataValue* Find(const UEdGraphPin* Pin);

	/** Get the PinData of the current pin. The top pin in the stack. */
	FPinDataValue& GetCurrent();

	/** Pop the current pin from the stack and append all its PinData with to previous pin in the stack. */
	void Pop();

	/** Push a pin to the stack and create its PinData. */
	void Push(const UEdGraphPin* Pin);

private:
	/** Data for each pin. */
	TMap<const UEdGraphPin*, FPinDataValue> Data;

	/** Stack of pins. Used to know which pin the data belongs to during the graph traversal recursion. */
	TArray<const UEdGraphPin*> PinStack;
};

/** Graph cycle key.
 *
 * Pin is not enough since we can call multiple recursive functions with the same pin.
 * Each function has to have an unique identifier.
 */
struct FGraphCycleKey
{
	friend uint32 GetTypeHash(const FGraphCycleKey& Key);

	FGraphCycleKey(const UEdGraphPin& Pin, uint32 Id);

	bool operator==(const FGraphCycleKey& Other) const;
	
	/** Valid pin. */
	const UEdGraphPin& Pin;

	/** Unique id. */
	uint32 Id;
};

/** Graph Cycle scope.
 *
 * Detect a cycle during the graph traversal.
 */
class FGraphCycle
{
public:
	explicit FGraphCycle(const FGraphCycleKey&& Key, FMutableGraphGenerationContext &Context);
	~FGraphCycle();

	/** Return true if there is a cycle. */
	bool FoundCycle() const;
	
private:
	/** Graph traversal key. */
	FGraphCycleKey Key;

	/** Generation context. */
	FMutableGraphGenerationContext& Context;
};

/** Return the default value if there is a cycle. */
#define RETURN_ON_CYCLE(Pin, GenerationContext) \
	FGraphCycle GraphCycle(FGraphCycleKey(Pin, __COUNTER__), GenerationContext); \
	if (GraphCycle.FoundCycle()) \
	{ \
		return {}; \
	} \

struct FMutableGraphGenerationContext
{
	FMutableGraphGenerationContext(UCustomizableObject* CustomizableObject, class FCustomizableObjectCompiler* InCompiler, const FCompilationOptions& InOptions);
	UCustomizableObject* Object = nullptr;

	// Non-owned reference to the compiler object
	FCustomizableObjectCompiler* Compiler = nullptr;

	// Compilation options, including target platform
	FCompilationOptions Options;

	// Cache of generated pins per LOD
	TMap<FGeneratedKey, FGeneratedData> Generated;

	/** Set of all generated nodes. */
	TSet<UCustomizableObjectNode*> GeneratedNodes;

	// Cache of generated Node Tables
	TMap<FString, mu::TablePtr> GeneratedTables;

	// Cache of generated images, because sometimes they are reused by LOD, we use this as a second
	// level cache
	TMap<FGeneratedImageKey, mu::NodeImageConstantPtr> GeneratedImages;

    // Global morph selection overrides.
    TArray<FRealTimeMorphSelectionOverride> RealTimeMorphTargetsOverrides;

	// Mutable meshes already build for source UStaticMesh or USkeletalMesh.
	struct FGeneratedMeshData
	{
		struct FKey
		{
			/** Source mesh data. */
			const UObject* Mesh = nullptr;
			int LOD = 0;
			int MaterialIndex = 0;

			/** Flag used to generate this mesh. Bit mask of EMutableMeshConversionFlags */
			uint32 Flags = 0;

			bool operator==( const FKey& k ) const
			{
				return Mesh == k.Mesh && LOD == k.LOD && MaterialIndex == k.MaterialIndex 
					&& Flags == k.Flags;
			}
		};

		FKey Key;

		/** Generated mesh. */
		mu::MeshPtr Generated;
	};
	TArray<FGeneratedMeshData> GeneratedMeshes;

	// Stack of mesh generation flags. The last one is the currently valid.
	// The value is a bit mask of EMutableMeshConversionFlags
	TArray<uint32> MeshGenerationFlags;

	/** Find a mesh if already generated for a given source and flags. */
	mu::MeshPtr FindGeneratedMesh(const FGeneratedMeshData::FKey& Key);

	/** Adds to ParameterNamesMap the node Node to the array of elements with name Name */
	void AddParameterNameUnique(const UCustomizableObjectNode* Node, FString Name);

	// Check if the Id of the node Node already exists, if it's new adds it to NodeIds array, otherwise, returns new Id
	const FGuid GetNodeIdUnique(const UCustomizableObjectNode* Node);

	/** Generates new tags for the UCustomizableObjectNodeMeshClipWithMesh nodes which have assigned a CO in the
	* UCustomizableObjectNodeMeshClipWithMesh::CustomizableObjectToClipWith field
	* @return nothing */
	void GenerateClippingCOInternalTags();

	/** Check if the PhysicsAsset of a given SkeletalMesh has any SkeletalBodySetup with BoneNames not present in the
	* InSkeletalMesh's RefSkeleton, if so, adds the PhysicsAsset to the DiscartedPhysicsAssetMap to display a warning later on */
	//void CheckPhysicsAssetInSkeletalMesh(const USkeletalMesh* InSkeletalMesh);

	/** Get the reference skeletal mesh associated to the current mesh component being generated */
	FMutableComponentInfo& GetCurrentComponentInfo();


	TArray<FMutableComponentInfo> ComponentInfos;
	TArray<FMutableRefSkeletalMeshData> ReferenceSkeletalMeshesData;
	
	TArray<UMaterialInterface*> ReferencedMaterials;
	TArray<FName> ReferencedMaterialSlotNames;
	TArray<FGeneratedImageProperties> ImageProperties;
	TMap<FString, TArray<const UCustomizableObjectNode*>> ParameterNamesMap;
	TArray<const UCustomizableObjectNode*> NoNameNodeObjectArray;
	TMap<FString, FCustomizableObjectIdPair> GroupNodeMap;
	TMap<FString, FString> CustomizableObjectPathMap;
	TMap<FString, FParameterUIData> ParameterUIDataMap;
	TMap<FString, FParameterUIData> StateUIDataMap;
	TMultiMap<const UCustomizableObjectNodeObjectGroup*, FGroupProjectorTempData> ProjectorGroupMap;
	//TMap<UPhysicsAsset*, uint32> DiscartedPhysicsAssetMap;

	TArray<const USkeleton*> ReferencedSkeletons;

	// Used to aviod Nodes with duplicated ids
	TMap<FGuid, TArray<const UCustomizableObjectNode*>> NodeIdsMap;
	TMultiMap<const UCustomizableObject*, FGroupNodeIdsTempData> DuplicatedGroupNodeIds;

	// For a given material node (the key is node package path + node uid + image index in node) stores images generated for the same node at a higher quality LOD to reuse that image node
	TMap<FString, FGroupProjectorImageInfo> GroupProjectorLODCache;

	// Data structures used for clipping feature by assigning a Customizable Object
	// Map with pairs (Unreal Mutable material node Guid, array with its corresponding Mutable surface node) to add tags from clipping nodes
	TMap<class UCustomizableObjectNodeMaterial*, TArray<mu::NodeSurfaceNewPtr>> MapMaterialNodeToMutableSurfaceNodeArray;

	// Map with pairs (Unreal Mutable clip mesh node, array with its corresponding Mutable mesh modifier nodes) to add tags
	TMap<class UCustomizableObjectNodeMeshClipWithMesh*, TArray<mu::NodeModifierMeshClipWithMeshPtr>> MapClipMeshNodeToMutableClipMeshNodeArray;

	// Data used for MorphTarget reconstruction.
	TArray<FMorphTargetInfo> ContributingMorphTargetsInfo;
	TArray<FMorphTargetVertexData> MorphTargetReconstructionData;

	// Data used for Clothing reconstruction.
	TArray<FCustomizableObjectMeshToMeshVertData> ClothMeshToMeshVertData;
	TArray<FCustomizableObjectClothingAssetData> ContributingClothingAssetsData;

	// Hierarchy of current ComponentNew nodes, each stored for every LOD
	struct ObjectParent
	{
		TArray<TArray<mu::NodeComponentNewPtr>> ComponentsPerLOD;
	};
	TArray< ObjectParent > ComponentNewNode;

	int32 CurrentLOD = 0;
	int32 NumLODsInRoot = 0;
	int32 CurrentMeshComponent = 0;
	int32 NumMeshComponentsInRoot = 0;
	int32 CurrentTextureLODBias = 0;

	int32 FirstLODAvailable = MAX_MESH_LOD_COUNT;
	int32 NumMaxLODsToStream = MAX_MESH_LOD_COUNT;

	bool bEnableLODStreaming = true;

	// Based on the last object visited.
	ECustomizableObjectAutomaticLODStrategy CurrentAutoLODStrategy = ECustomizableObjectAutomaticLODStrategy::Manual;

	// Stores external graph root nodes to be added to the specified group nodes
	TMultiMap<FGuid, UCustomizableObjectNodeObject*> GroupIdToExternalNodeMap;

	// Easily retrieve a parameter name from its node guid
	TMap<FGuid, FString> GuidToParamNameMap;

	// Graph cycle detection
	/** Visited nodes during the DAC recursion traversal.
	 * It acts like stack, pushing pins when recursively exploring a new pin an popping it when exiting the recursion. */
	TMap<FGraphCycleKey, const UCustomizableObject*> VisitedPins;
	const UCustomizableObject* CustomizableObjectWithCycle = nullptr;

	// Texture management flag, captured from the real object root node
	bool bDisableTextureLayoutManagementFlag = false;

	// Set of all the guids of all the child CustomizableObjects in the compilation
	TSet<FGuid> CustomizableObjectGuidsInCompilation;

	/** Array with the conversion tasks for each Unreal texture that needs to be converted to Mutable format for a particular Customizable Object.
	* Multiple tasks for the same texture may be added here when referenced from different LODs or nodes, however when precessing the array it'll
	* make sure to avoid converting the same texture more than once.
	*/
	TArray<FTextureUnrealToMutableTask> ArrayTextureUnrealToMutableTask;

	/** Stores the physics assets gathered from the SkeletalMesh nodes during compilation, to be used in mesh generation in-game */
	TMap<FString, TSoftObjectPtr<UPhysicsAsset>> PhysicsAssetMap;

	/** Stores the anim BP assets gathered from the SkeletalMesh nodes during compilation, to be used in mesh generation in-game */
	TMap<FString, TSoftClassPtr<UAnimInstance>> AnimBPAssetsMap;

	// Stores the textures that will be used to mask-out areas in the projection. The cache isn't used for rendering, but for coverage testing
	TMap<FString, FString> MaskOutMaterialCache; // Maps a UMaterial's asset path to a UTexture's asset path
	TMap<FString, FMaskOutTexture> MaskOutTextureCache; // Maps a UTexture's asset path to the cached mask-out texture data

	// Stores the only option of an Int Param that should be compiled in a partial compilation
	TMap<FString, FString> ParamNamesToSelectedOptions;

	TArray<const UEdGraphNode*> LimitedParameters;
	int32 ParameterLimitationCount = 0;

	/** Data which is behind a given output pin. 
	 *
	 * We only consider output pins. Do not use input pins as keys. See FPinData class definition. 
	 */
	FPinData PinData; // DEPRECATED. DO NOT USE!

	// Stores all morphs to apply them directly to a skeletal mesh node
	TArray<UCustomizableObjectNodeMeshMorph*> MeshMorphStack;

	// Current material parameter name to find the corresponding column in a mutable table
	FString CurrentMaterialTableParameter;

	// Current material parameter id to find the corresponding column in a mutable table
	FString CurrentMaterialTableParameterId;

	// Stores the parameters generated in the node tables
	TMap<const class UCustomizableObjectNodeTable*, TArray<FGuid>> GeneratedParametersInTables;

	struct FMeshWithBoneRemovalApplied
	{
		TObjectPtr<class USkeletalMesh> Mesh;
		
		// Key is LOD index
		//
		// This is a cache. Entries are added on demand. A processed LOD with no bones removed will have an empty entry.
		TMap<int32, TMap<int32, int32>> RemovedBonesActiveParentIndicesPerLOD;

		bool bHasBonesToRemove = false;
	};

	TMap<TObjectPtr<class USkeletalMesh>, FMeshWithBoneRemovalApplied> MeshesWithBoneRemovalApplied;
};

/** Pin Data scope wrapper. Pops the pin data on scope exit. */
class FScopedPinData
{
public:
	explicit FScopedPinData(FMutableGraphGenerationContext& Context, const UEdGraphPin* Pin);
	~FScopedPinData();

private:
	FMutableGraphGenerationContext& Context;
};

#define SCOPED_PIN_DATA(Context, Pin) \
	FScopedPinData ScopedPinData(Context, Pin);


//
mu::NodeObjectPtr GenerateMutableSource(const class UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, bool bPartialCompilation);


/** Populate an array with all the information related to the reference skeletal meshes we might need in-game to generate instances */
void PopulateReferenceSkeletalMeshesData(FMutableGraphGenerationContext& GenerationContext);


void CheckNumOutputs(const UEdGraphPin& Pin, const FMutableGraphGenerationContext& GenerationContext);


// TODO GMT Remove generation context dependency and move to GraphTraversal.
UTexture2D* FindReferenceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);


mu::NodeMeshApplyPosePtr CreateNodeMeshApplyPose(mu::NodeMeshPtr InputMeshNode, UCustomizableObject * CustomizableObject, TArray<FString> ArrayBoneName, TArray<FTransform> ArrayTransform);


// Generates the tag for an animation instance
FString GenerateAnimationInstanceTag(const FString& AnimInstance, int32 SlotIndex);


FString GenerateGameplayTag(const FString& GameplayTag);

// Computes the LOD bias for a texture given the current mesh LOD and automatic LOD settings, the reference texture settings
// and whether it's being built for a server or not
int32 ComputeLODBias(const FMutableGraphGenerationContext& GenerationContext, const UTexture2D* ReferenceTexture, int32 MaxTextureSize,
	const UCustomizableObjectNodeMaterial* MaterialNode, const int32 ImageIndex);


int32 GetMaxTextureSize(const UTexture2D* ReferenceTexture, const FMutableGraphGenerationContext& GenerationContext);
