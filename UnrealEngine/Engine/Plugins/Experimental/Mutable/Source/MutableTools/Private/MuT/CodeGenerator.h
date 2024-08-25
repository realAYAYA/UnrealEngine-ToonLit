// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableMemory.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/Compiler.h"
#include "MuT/ErrorLog.h"
#include "MuT/Node.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeExtensionData.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeString.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"
#include "Templates/TypeHash.h"


namespace mu
{
	class ASTOpParameter;
	class Layout;
	class NodeColourArithmeticOperation;
	class NodeColourConstant;
	class NodeColourFromScalars;
	class NodeColourParameter;
	class NodeColourSwitch;
	class NodeColourTable;
	class NodeColourVariation;
	class NodeImageBinarise;
	class NodeImageColourMap;
	class NodeImageConditional;
	class NodeImageConstant;
	class NodeImageFormat;
	class NodeImageGradient;
	class NodeImageInterpolate;
	class NodeImageInvert;
	class NodeImageLayer;
	class NodeImageLayerColour;
	class NodeImageLuminance;
	class NodeImageMipmap;
	class NodeImageMultiLayer;
	class NodeImageNormalComposite;
	class NodeImageParameter;
	class NodeImagePlainColour;
	class NodeImageResize;
	class NodeImageSaturate;
	class NodeImageSwitch;
	class NodeImageSwizzle;
	class NodeImageTable;
	class NodeImageTransform;
	class NodeImageVariation;
	class NodeMeshApplyPose;
	class NodeMeshClipDeform;
	class NodeMeshClipMorphPlane;
	class NodeMeshClipWithMesh;
	class NodeMeshConstant;
	class NodeMeshFormat;
	class NodeMeshFragment;
	class NodeMeshInterpolate;
	class NodeMeshMakeMorph;
	class NodeMeshMorph;
	class NodeMeshReshape;
	class NodeMeshSwitch;
	class NodeMeshTransform;
	class NodeMeshVariation;
	class NodeRange;
	class NodeScalarArithmeticOperation;
	class NodeScalarConstant;
	class NodeScalarCurve;
	class NodeScalarEnumParameter;
	class NodeScalarParameter;
	class NodeScalarSwitch;
	class NodeScalarTable;
	class NodeScalarVariation;
	class NodeStringConstant;
	class NodeStringParameter;
	struct FObjectState;
	struct FProgram;



    //---------------------------------------------------------------------------------------------
    //! Code generator
    //---------------------------------------------------------------------------------------------
    class CodeGenerator
    {
		
		friend class FirstPassGenerator;

    public:

        CodeGenerator( CompilerOptions::Private* options );

        //! Data will be stored in m_states
        void GenerateRoot( const Ptr<const Node> );

	public:

		// Generic top-level nodes
		struct FGenericGenerationOptions
		{
			friend FORCEINLINE uint32 GetTypeHash(const FGenericGenerationOptions& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.State));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.ActiveTags.Num()));
				return KeyHash;
			}

			bool operator==(const FGenericGenerationOptions& InKey) const
			{
				if (State != InKey.State) return false;
				if (ActiveTags != InKey.ActiveTags) return false;
				return true;
			}

			int32 State = -1;
			TArray<FString> ActiveTags;
		};

		struct FGenericGenerationResult
		{
			Ptr<ASTOp> op;
		};

		struct FGeneratedCacheKey
		{
			Ptr<const Node> Node;
			FGenericGenerationOptions Options;

			friend FORCEINLINE uint32 GetTypeHash(const FGeneratedCacheKey& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.Node.get()));
				KeyHash = HashCombineFast(KeyHash, GetTypeHash(InKey.Options));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FGeneratedCacheKey& Other) const
			{
				return Node == Other.Node && Options == Other.Options;
			}
		};

		typedef TMap<FGeneratedCacheKey, FGenericGenerationResult> FGeneratedGenericNodesMap;
		FGeneratedGenericNodesMap GeneratedGenericNodes;

		Ptr<ASTOp> Generate(const Ptr<const Node>, const FGenericGenerationOptions& );
		void Generate_ComponentNew(const FGenericGenerationOptions&, FGenericGenerationResult&, const NodeComponentNew*);
		void Generate_LOD(const FGenericGenerationOptions&, FGenericGenerationResult&, const NodeLOD*);
		void Generate_ObjectNew(const FGenericGenerationOptions&, FGenericGenerationResult&, const NodeObjectNew*);
		void Generate_ObjectGroup(const FGenericGenerationOptions&, FGenericGenerationResult&, const NodeObjectGroup*);

    public:

        //! Settings
        CompilerOptions::Private* m_compilerOptions = nullptr;

		//!
		FirstPassGenerator m_firstPass;

        //!
        ErrorLogPtr m_pErrorLog;

        //! After the entire code generation this contains the information about all the states
        typedef TArray< TPair<FObjectState, Ptr<ASTOp>> > StateList;
        StateList m_states;

    private:

        //! List of meshes generated to be able to reuse them
		TArray<Ptr<Mesh>> m_constantMeshes;

        //! List of image resources for every image formata that have been generated so far as
        //! palceholders for missing images.
        Ptr<Image> m_missingImage[size_t(EImageFormat::IF_COUNT)];

        //! First free index for a layout block
        int32 m_absoluteLayoutIndex = 0;

        //! First free index to be used to identify mesh vertices.
        uint32 m_freeVertexIndex = 0;

		// (top-down) Tags that are active when generating nodes.
		TArray< TArray<FString> > m_activeTags;

        struct FParentKey
        {
            FParentKey()
            {
                m_pObject = nullptr;
                m_state = -1;
                m_lod = -1;
                m_component = -1;
                m_surface = -1;
                m_texture = -1;
                m_block = -1;
            }

            const NodeObjectNew::Private* m_pObject;
            int m_state;
            int m_lod;
            int m_component;
            int m_surface;
            int m_texture;
            int m_block;
        };

		TArray< FParentKey > m_currentParents;

        // List of additional components to add to an object that come from child objects.
        // The index is the object and lod that should receive the components.
        struct FAdditionalComponentKey
        {
			FAdditionalComponentKey()
            {
                m_pObject = nullptr;
                m_lod = -1;
            }

            const NodeObjectNew::Private* m_pObject;
            int32 m_lod;

			FORCEINLINE bool operator==(const FAdditionalComponentKey& Other) const
			{
				return m_pObject == Other.m_pObject
					&&
					m_lod == Other.m_lod;
			}

			friend FORCEINLINE uint32 GetTypeHash(const FAdditionalComponentKey& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.m_pObject));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.m_lod));
				return KeyHash;
			}
		};
        TMap< FAdditionalComponentKey, TArray<Ptr<ASTOp>> > AdditionalComponents;


        struct FObjectGenerationData
        {
            // Condition that enables a specific object
            Ptr<ASTOp> m_condition;
        };
		TArray<FObjectGenerationData> m_currentObject;

		/** The key for generated tables is made of the source table and a parameter name. */
		struct FTableCacheKey
		{
			Ptr<const Table> Table;
			FString ParameterName;

			friend FORCEINLINE uint32 GetTypeHash(const FTableCacheKey& InKey)
			{
				uint32 KeyHash = ::GetTypeHash(InKey.Table.get());
				KeyHash = HashCombineFast(KeyHash, GetTypeHash(InKey.ParameterName));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FTableCacheKey& InKey) const
			{
				if (Table != InKey.Table) return false;
				if (ParameterName != InKey.ParameterName) return false;
				return true;
			}
		};
		TMap< FTableCacheKey, Ptr<ASTOp> > GeneratedTables;

		struct FConditionalExtensionDataOp
		{
			Ptr<ASTOp> Condition;
			Ptr<ASTOp> ExtensionDataOp;
			FString ExtensionDataName;
		};

		TArray<FConditionalExtensionDataOp> ConditionalExtensionDataOps;

		//-----------------------------------------------------------------------------------------

		// Get the modifiers that have to be applied to elements with a specific tag.
		void GetModifiersFor(const TArray<FString>& SurfaceTags, int32 LOD,
			bool bModifiersForBeforeOperations, TArray<FirstPassGenerator::FModifier>& OutModifiers);

		// Apply the required mesh modifiers to the given operation.
		Ptr<ASTOp> ApplyMeshModifiers(const FGenericGenerationOptions&, const Ptr<ASTOp>& SourceOp,
			bool bModifiersForBeforeOperations, const void* errorContext);

        //-----------------------------------------------------------------------------------------
        //!
        Ptr<ASTOp> GenerateTableVariable(Ptr<const Node>, const FTableCacheKey&, bool bAddNoneOption, const FString& DefaultRowName);

        //!
        Ptr<ASTOp> GenerateMissingBoolCode(const TCHAR* strWhere, bool value, const void* errorContext );

        //!
		template<class NODE_TABLE_PRIVATE, ETableColumnType TYPE, OP_TYPE OPTYPE, typename F>
		Ptr<ASTOp> GenerateTableSwitch( const NODE_TABLE_PRIVATE& node, F&& GenerateOption );


		//-----------------------------------------------------------------------------------------
		// Images
		
		/** Options that affect the generation of images. It is like list of what required data we want while parsing down the image node graph. */
		struct FImageGenerationOptions : public FGenericGenerationOptions
		{
			/** */
			CompilerOptions::TextureLayoutStrategy ImageLayoutStrategy = CompilerOptions::TextureLayoutStrategy::None;

			/** If different than {0,0} this is the mandatory size of the image that needs to be generated. */
			UE::Math::TIntVector2<int32> RectSize = {0, 0};

			/** Layout block that we are trying to generate if any. */
			int32 LayoutBlockId = -1;
			Ptr<const Layout> LayoutToApply;

			friend FORCEINLINE uint32 GetTypeHash(const FImageGenerationOptions& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.ImageLayoutStrategy));
				KeyHash = HashCombineFast(KeyHash, GetTypeHash(InKey.RectSize));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.State));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.LayoutBlockId));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.LayoutToApply.get()));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FImageGenerationOptions& Other) const
			{
				return ImageLayoutStrategy == Other.ImageLayoutStrategy
					&&
					RectSize == Other.RectSize
					&&
					State == Other.State
					&&
					LayoutBlockId == Other.LayoutBlockId
					&&
					LayoutToApply == Other.LayoutToApply
					&&
					ActiveTags == Other.ActiveTags;
			}

		};

		/** */
		struct FImageGenerationResult
		{
			Ptr<ASTOp> op;
		};

		/** */
		struct FGeneratedImageCacheKey
		{
			NodePtrConst Node;
			FImageGenerationOptions Options;

			friend FORCEINLINE uint32 GetTypeHash(const FGeneratedImageCacheKey& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.Node.get()));
				KeyHash = HashCombineFast(KeyHash, GetTypeHash(InKey.Options));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FGeneratedImageCacheKey& Other) const
			{
				return Node == Other.Node && Options == Other.Options;
			}
		};

		typedef TMap<FGeneratedImageCacheKey, FImageGenerationResult> GeneratedImagesMap;
		GeneratedImagesMap m_generatedImages;

		void GenerateImage(const FImageGenerationOptions&, FImageGenerationResult& result, const NodeImagePtrConst& node);
		void GenerateImage_Constant(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageConstant*);
		void GenerateImage_Interpolate(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageInterpolate*);
		void GenerateImage_Saturate(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageSaturate*);
		void GenerateImage_Table(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageTable*);
		void GenerateImage_Swizzle(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageSwizzle*);
		void GenerateImage_ColourMap(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageColourMap*);
		void GenerateImage_Gradient(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageGradient*);
		void GenerateImage_Binarise(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageBinarise*);
		void GenerateImage_Luminance(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageLuminance*);
		void GenerateImage_Layer(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageLayer*);
		void GenerateImage_LayerColour(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageLayerColour*);
		void GenerateImage_Resize(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageResize*);
		void GenerateImage_PlainColour(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImagePlainColour*);
		void GenerateImage_Project(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageProject*);
		void GenerateImage_Mipmap(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageMipmap*);
		void GenerateImage_Switch(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageSwitch*);
		void GenerateImage_Conditional(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageConditional*);
		void GenerateImage_Format(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageFormat*);
		void GenerateImage_Parameter(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageParameter*);
		void GenerateImage_MultiLayer(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageMultiLayer*);
		void GenerateImage_Invert(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageInvert*);
		void GenerateImage_Variation(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageVariation*);
		void GenerateImage_NormalComposite(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageNormalComposite*);
		void GenerateImage_Transform(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageTransform*);

		//!
		Ptr<Image> GenerateMissingImage(EImageFormat);

		//!
		Ptr<ASTOp> GenerateMissingImageCode(const TCHAR* strWhere, EImageFormat, const void* errorContext, const FImageGenerationOptions& Options);

		//!
		Ptr<ASTOp> GeneratePlainImageCode(const FVector4f& Color, const FImageGenerationOptions& Options);

		//!
		Ptr<ASTOp> GenerateImageFormat(Ptr<ASTOp>, EImageFormat);

		//!
		Ptr<ASTOp> GenerateImageUncompressed(Ptr<ASTOp>);

		//!
		Ptr<ASTOp> GenerateImageSize(Ptr<ASTOp>, UE::Math::TIntVector2<int32>);

		/** Evaluate if the image to generate is big enough to be split in separate operations and tiled afterwards. */
		Ptr<ASTOp> ApplyTiling(Ptr<ASTOp> Source, UE::Math::TIntVector2<int32> Size, EImageFormat Format);

		//!
		Ptr<ASTOp> GenerateImageBlockPatch(Ptr<ASTOp> blockAd, const NodePatchImage* pPatch, Ptr<ASTOp> conditionAd, const FImageGenerationOptions& ImageOptions);

        //-----------------------------------------------------------------------------------------
        // Meshes

		/** Options that affect the generation of meshes. It is like list of what required data we want
		* while parsing down the mesh node graph.
		*/
		struct FMeshGenerationOptions : public FGenericGenerationOptions
		{
			/** Whatever mesh we reach at the leaves of the graph will need to have unique ids for its vertices.
			* This is used to track mesh removal indices, morph data in other nodes, clothing data, etc.
			*/
			bool bUniqueVertexIDs = false;

			/** The meshes at the leaves will need their own layout block data. */
			bool bLayouts = false;

			/** If true, Ensure UV Islands are not split between two or more blocks. UVs shared between multiple 
			* layout blocks will be clamped to fit the one with more vertices belonging to the UV island.
			* Mainly used to keep consistent layouts when reusing textures between LODs. */
			bool bClampUVIslands = false;

			/** If true, UVs will be normalized. Normalize UVs should be done in cases where we operate with Images and Layouts */
			bool bNormalizeUVs = false;

			/** If this has something the layouts in constant meshes will be ignored, because
			* they are supposed to match some other set of layouts. If the array is empty, layouts
			* are generated normally.
			*/
			TArray<Ptr<const Layout>> OverrideLayouts;			

			/** Optional context to use instead of the node error context.
			 * Be careful since it is not used everywhere. Check usages before assigning a value to it. */
			TOptional<const void*> OverrideContext;
			
			friend FORCEINLINE uint32 GetTypeHash(const FMeshGenerationOptions& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.bUniqueVertexIDs));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.bLayouts));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.OverrideLayouts.Num()));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.State));
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.ActiveTags.Num()));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FMeshGenerationOptions& Other) const
			{
				return State==Other.State 
					&& bUniqueVertexIDs==Other.bUniqueVertexIDs && bLayouts==Other.bLayouts 
					&& bClampUVIslands == Other.bClampUVIslands && bNormalizeUVs == Other.bNormalizeUVs
					&& ActiveTags==Other.ActiveTags
					&& OverrideLayouts ==Other.OverrideLayouts;
			}
		};

		//! Store the results of the code generation of a mesh.
		struct FMeshGenerationResult
		{
			//! Mesh after all code tree is applied
			Ptr<ASTOp> meshOp;

			//! Original base mesh before removes, morphs, etc.
			Ptr<ASTOp> baseMeshOp;

			/** Generated node layouts with their own block ids. */
			TArray<Ptr<const Layout>> GeneratedLayouts;

			TArray<Ptr<ASTOp>> layoutOps;

			struct FExtraLayouts
			{
				/** Source node layouts to use with these extra mesh. They don't have block ids. */
				TArray<Ptr<const Layout>> GeneratedLayouts;
				Ptr<ASTOp> condition;
				Ptr<ASTOp> meshFragment;
			};
			TArray< FExtraLayouts > extraMeshLayouts;
		};
		
		struct FGeneratedMeshCacheKey
		{
			NodePtrConst Node;
			FMeshGenerationOptions Options;

			friend FORCEINLINE uint32 GetTypeHash(const FGeneratedMeshCacheKey& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombineFast(KeyHash, ::GetTypeHash(InKey.Node.get()));
				KeyHash = HashCombineFast(KeyHash, GetTypeHash(InKey.Options));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FGeneratedMeshCacheKey& Other) const
			{
				return Node == Other.Node && Options == Other.Options;
			}
		};

        typedef TMap<FGeneratedMeshCacheKey,FMeshGenerationResult> GeneratedMeshMap;
        GeneratedMeshMap m_generatedMeshes;

		/** Store the mesh generation data for surfaces that we intend to share across LODs. 
		* The key is the SharedSurfaceId.
		*/
		TMap<int32, FMeshGenerationResult> SharedMeshOptionsMap;

		//! Map of layouts found in the code already generated. The map is from the source layout
		//! pointer of the layouts in the node meshes to the cloned and modified layout. 
		//! The cloned layout will have absolute block ids assigned.
		TMap<Ptr<const Layout>, Ptr<const Layout>> GeneratedLayouts;

        void GenerateMesh(const FMeshGenerationOptions&, FMeshGenerationResult& result, const NodeMeshPtrConst&);
        void GenerateMesh_Constant(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshConstant* );
        void GenerateMesh_Format(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshFormat* );
        void GenerateMesh_Morph(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshMorph* );
        void GenerateMesh_MakeMorph(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshMakeMorph* );
        void GenerateMesh_Fragment(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshFragment* );
        void GenerateMesh_Interpolate(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshInterpolate* );
        void GenerateMesh_Switch(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshSwitch* );
        void GenerateMesh_Transform(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshTransform* );
        void GenerateMesh_ClipMorphPlane(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipMorphPlane* );
        void GenerateMesh_ClipWithMesh(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipWithMesh* );
        void GenerateMesh_ApplyPose(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshApplyPose* );
        void GenerateMesh_Variation(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshVariation* );
		void GenerateMesh_Table(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshTable*);
		void GenerateMesh_GeometryOperation(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshGeometryOperation*);
		void GenerateMesh_Reshape(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshReshape*);
		void GenerateMesh_ClipDeform(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipDeform*);

		//-----------------------------------------------------------------------------------------
		void PrepareForLayout( Ptr<const Layout> GeneratedLayout,
			Ptr<Mesh> currentLayoutMesh,
			int32 currentLayoutChannel,
			const void* errorContext,
			const FMeshGenerationOptions& MeshOptions);

		//!
		Ptr<const Layout> AddLayout(Ptr<const Layout> SourceLayout);

		struct FExtensionDataGenerationResult
		{
			Ptr<ASTOp> Op;
		};

		typedef const NodeExtensionData* FGeneratedExtensionDataCacheKey;
		typedef TMap<FGeneratedExtensionDataCacheKey, FExtensionDataGenerationResult> FGeneratedExtensionDataMap;
		FGeneratedExtensionDataMap GeneratedExtensionData;

		void GenerateExtensionData(FExtensionDataGenerationResult& OutResult, const FGenericGenerationOptions&, const NodeExtensionDataPtrConst&);
		void GenerateExtensionData_Constant(FExtensionDataGenerationResult& OutResult, const FGenericGenerationOptions&, const class NodeExtensionDataConstant*);
		void GenerateExtensionData_Switch(FExtensionDataGenerationResult& OutResult, const FGenericGenerationOptions&, const class NodeExtensionDataSwitch*);
		void GenerateExtensionData_Variation(FExtensionDataGenerationResult& OutResult, const FGenericGenerationOptions&, const class NodeExtensionDataVariation*);
		Ptr<ASTOp> GenerateMissingExtensionDataCode(const TCHAR* StrWhere, const void* ErrorContext);

        //-----------------------------------------------------------------------------------------
        // Projectors
        struct FProjectorGenerationResult
        {
            Ptr<ASTOp> op;
            PROJECTOR_TYPE type;
        };

        typedef TMap<FGeneratedCacheKey,FProjectorGenerationResult> FGeneratedProjectorsMap;
        FGeneratedProjectorsMap GeneratedProjectors;

        void GenerateProjector( FProjectorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeProjector>& );
        void GenerateProjector_Constant( FProjectorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeProjectorConstant>& );
        void GenerateProjector_Parameter( FProjectorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeProjectorParameter>& );
        void GenerateMissingProjectorCode( FProjectorGenerationResult&, const void* errorContext );

		//-----------------------------------------------------------------------------------------
		// Bools
		struct FBoolGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FGeneratedCacheKey, FBoolGenerationResult> FGeneratedBoolsMap;
		FGeneratedBoolsMap GeneratedBools;

		void GenerateBool(FBoolGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeBool>&);
		void GenerateBool_Constant(FBoolGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeBoolConstant>&);
		void GenerateBool_Parameter(FBoolGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeBoolParameter>&);
		void GenerateBool_Not(FBoolGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeBoolNot>&);
		void GenerateBool_And(FBoolGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeBoolAnd>&);

		//-----------------------------------------------------------------------------------------
		// Scalars
		struct FScalarGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FGeneratedCacheKey, FScalarGenerationResult> FGeneratedScalarsMap;
		FGeneratedScalarsMap GeneratedScalars;

		void GenerateScalar(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalar>&);
		void GenerateScalar_Constant(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarConstant>&);
		void GenerateScalar_Parameter(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarParameter>&);
		void GenerateScalar_Switch(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarSwitch>&);
		void GenerateScalar_EnumParameter(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarEnumParameter>&);
		void GenerateScalar_Curve(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarCurve>&);
		void GenerateScalar_Arithmetic(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarArithmeticOperation>&);
		void GenerateScalar_Variation(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarVariation>&);
		void GenerateScalar_Table(FScalarGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeScalarTable>&);
		Ptr<ASTOp> GenerateMissingScalarCode(const TCHAR* strWhere, float value, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Colors
		struct FColorGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FGeneratedCacheKey, FColorGenerationResult> FGeneratedColorsMap;
		FGeneratedColorsMap GeneratedColors;

		void GenerateColor(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColour>&);
		void GenerateColor_Constant(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourConstant>&);
		void GenerateColor_Parameter(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourParameter>&);
		void GenerateColor_Switch(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourSwitch>&);
		void GenerateColor_SampleImage(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourSampleImage>&);
		void GenerateColor_FromScalars(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourFromScalars>&);
		void GenerateColor_Arithmetic(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourArithmeticOperation>&);
		void GenerateColor_Variation(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourVariation>&);
		void GenerateColor_Table(FColorGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeColourTable>&);
		Ptr<ASTOp> GenerateMissingColourCode(const TCHAR* strWhere, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Strings
		struct FStringGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FGeneratedCacheKey, FStringGenerationResult> FGeneratedStringsMap;
		FGeneratedStringsMap GeneratedStrings;

		void GenerateString(FStringGenerationResult&, const FGenericGenerationOptions&, const Ptr<const NodeString>&);
		void GenerateString_Constant(FStringGenerationResult&, const FGenericGenerationOptions& Options, const Ptr<const NodeStringConstant>&);
		void GenerateString_Parameter(FStringGenerationResult&, const FGenericGenerationOptions& Options, const Ptr<const NodeStringParameter>&);

        //-----------------------------------------------------------------------------------------
        // Ranges
        struct FRangeGenerationResult
        {
            //
            Ptr<ASTOp> sizeOp;

            //
            FString rangeName;

            //
			FString rangeUID;
        };

        typedef TMap<FGeneratedCacheKey,FRangeGenerationResult> FGeneratedRangeMap;
        FGeneratedRangeMap GeneratedRanges;

        void GenerateRange(FRangeGenerationResult&, const FGenericGenerationOptions&, Ptr<const NodeRange>);


        //-----------------------------------------------------------------------------------------
        struct FSurfaceGenerationResult
        {
            Ptr<ASTOp> surfaceOp;
        };

        void GenerateSurface( FSurfaceGenerationResult&, const FGenericGenerationOptions&,
                              Ptr<const NodeSurfaceNew>,
                              const TArray<FirstPassGenerator::FSurface::FEdit>& Edits );

		//-----------------------------------------------------------------------------------------
		//Default Table Parameters
		Ptr<ASTOp> GenerateDefaultTableValue(ETableColumnType NodeType);
    };

	
    //---------------------------------------------------------------------------------------------
    template<class NODE_TABLE_PRIVATE, ETableColumnType TYPE, OP_TYPE OPTYPE, typename F>
    Ptr<ASTOp> CodeGenerator::GenerateTableSwitch( const NODE_TABLE_PRIVATE& node, F&& GenerateOption )
    {
        Ptr<const Table> NodeTable = node.Table;
        Ptr<ASTOp> Variable;

		FTableCacheKey CacheKey = FTableCacheKey{ node.Table, node.ParameterName };
        Ptr<ASTOp>* it = GeneratedTables.Find( CacheKey );
        if ( it )
        {
            Variable = *it;
        }

        if ( !Variable)
        {
            // Create the table variable expression
            Variable = GenerateTableVariable( node.m_pNode, CacheKey, node.bNoneOption, node.DefaultRowName);

            GeneratedTables.Add(CacheKey, Variable );
        }

		int32 NumRows = NodeTable->GetPrivate()->Rows.Num();

        // Verify that the table column is the right type
        int32 ColIndex = NodeTable->FindColumn( node.ColumnName );

		if (NumRows == 0)
		{
			m_pErrorLog->GetPrivate()->Add("The table has no rows.", ELMT_ERROR, node.m_errorContext);
			return nullptr;
		}
        else if (ColIndex < 0)
        {
            m_pErrorLog->GetPrivate()->Add("Table column not found.", ELMT_ERROR, node.m_errorContext);
            return nullptr;
        }

        if (NodeTable->GetPrivate()->Columns[ ColIndex ].Type != TYPE )
        {
            m_pErrorLog->GetPrivate()->Add("Table column type is not the right type.", ELMT_ERROR, node.m_errorContext);
            return nullptr;
        }

        // Create the switch to cover all the options
        Ptr<ASTOp> lastSwitch;
        Ptr<ASTOpSwitch> SwitchOp = new ASTOpSwitch();
		SwitchOp->type = OPTYPE;
		SwitchOp->variable = Variable;
		SwitchOp->def = GenerateDefaultTableValue(TYPE);

		for (int32 i = 0; i < NumRows; ++i)
        {
            check(NodeTable->GetPrivate()->Rows[i].Id <= 0xFFFF);
            auto Condition = (uint16)NodeTable->GetPrivate()->Rows[i].Id;
            Ptr<ASTOp> Branch = GenerateOption( node, ColIndex, (int)i, m_pErrorLog.get() );

			if (Branch || TYPE != ETableColumnType::Mesh)
			{
				SwitchOp->cases.Add(ASTOpSwitch::FCase(Condition, SwitchOp, Branch));
			}
        }

        return SwitchOp;
    }
}
