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
#include "MuT/Visitor.h"
#include "Templates/TypeHash.h"

#include <utility>


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
	class NodeImageReference;
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
    class CodeGenerator : public Base,
                          public BaseVisitor,

                          public Visitor<NodeComponentNew::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeComponentEdit::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeLOD::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeObjectNew::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeObjectGroup::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodePatchImage::Private, Ptr<ASTOp>, true>
    {
    public:

        CodeGenerator( CompilerOptions::Private* options );

        //! Data will be stored in m_states
        void GenerateRoot( const NodePtrConst pNode );

	protected:

        Ptr<ASTOp> Generate(const NodePtrConst pNode);

	public:

        Ptr<ASTOp> Visit( const NodeComponentNew::Private& ) override;
        Ptr<ASTOp> Visit( const NodeComponentEdit::Private& ) override;
        Ptr<ASTOp> Visit( const NodeLOD::Private& ) override;
        Ptr<ASTOp> Visit( const NodeObjectNew::Private& ) override;
        Ptr<ASTOp> Visit( const NodeObjectGroup::Private& ) override;
        Ptr<ASTOp> Visit( const NodePatchImage::Private& ) override;

    public:

        //! Settings
        CompilerOptions::Private* m_compilerOptions = nullptr;

		//!
		FirstPassGenerator m_firstPass;

        struct FVisitedKeyMap
        {
            FVisitedKeyMap()
            {
            }

			friend FORCEINLINE uint32 GetTypeHash(const FVisitedKeyMap& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.pNode.get()));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.state));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash((uint64)InKey.activeTags.Num()));
				return KeyHash;
			}

			bool operator==(const FVisitedKeyMap& InKey) const
			{
				if (pNode != InKey.pNode) return false;
				if (state != InKey.state) return false;
				if (activeTags != InKey.activeTags) return false;
				return true;
			}

            // This reference has to be the smart pointer to avoid memory aliasing, keeping
            // processed nodes alive.
            NodePtrConst pNode;
            int state = -1;
			TArray<mu::string> activeTags;
        };

        //! This struct contains additional state propagated from bottom to top of the object node graph.
        //! It is stored for every visited node, and restored when the cache is used.
        struct BOTTOM_UP_STATE
        {
            //! Generated root address for the node.
            Ptr<ASTOp> m_address;
        };
        BOTTOM_UP_STATE m_currentBottomUpState;

        typedef TMap<FVisitedKeyMap,BOTTOM_UP_STATE> VisitedMap;
        VisitedMap m_compiled;

        //!
        ErrorLogPtr m_pErrorLog;

        //! While generating code, this contains the index of the state being generated. This
        //! can only be used with the state data in m_firstPass.
        int m_currentStateIndex = -1;

        //! After the entire code generation this contains the information about all the states
        typedef TArray< std::pair<FObjectState, Ptr<ASTOp>> > StateList;
        StateList m_states;

    private:

        //! List of meshes generated to be able to reuse them
		TArray<MeshPtr> m_constantMeshes;

        //! List of image resources for every image formata that have been generated so far as
        //! palceholders for missing images.
        ImagePtr m_missingImage[size_t(EImageFormat::IF_COUNT)];

        //! First free index for a layout block
        int32 m_absoluteLayoutIndex = 0;

        //! First free index to be used to identify mesh vertices.
        uint32 m_freeVertexIndex = 0;

		// (top-down) Tags that are active when generating nodes.
		TArray< TArray<mu::string> > m_activeTags;

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
        struct ADDITIONAL_COMPONENT_KEY
        {
            ADDITIONAL_COMPONENT_KEY()
            {
                m_pObject = nullptr;
                m_lod = -1;
            }

            const NodeObjectNew::Private* m_pObject;
            int m_lod;

            inline bool operator<(const ADDITIONAL_COMPONENT_KEY& o) const
            {
                if (m_pObject < o.m_pObject) return true;
                if (m_pObject > o.m_pObject) return false;
                return m_lod < o.m_lod;
            }
        };
        std::map< ADDITIONAL_COMPONENT_KEY, TArray<Ptr<ASTOp>> > m_additionalComponents;


        struct OBJECT_GENERATION_DATA
        {
            // Condition that enables a specific object
            Ptr<ASTOp> m_condition;
        };
		TArray< OBJECT_GENERATION_DATA > m_currentObject;

        map< std::pair<TablePtr,string>, std::pair<TablePtr,Ptr<ASTOp>> > m_generatedTables;

        //! Variables added for every node
        map< Ptr<const Node>, Ptr<ASTOpParameter> > m_nodeVariables;

		struct FConditionalExtensionDataOp
		{
			Ptr<ASTOp> Condition;
			Ptr<ASTOp> ExtensionDataOp;
			string ExtensionDataName;
		};

		TArray<FConditionalExtensionDataOp> m_conditionalExtensionDataOps;

		//-----------------------------------------------------------------------------------------

		// Get the modifiers that have to be applied to elements with a specific tag.
		void GetModifiersFor(const TArray<string>& tags, int LOD,
			bool bModifiersForBeforeOperations, TArray<FirstPassGenerator::MODIFIER>& modifiers);

		// Apply the required mesh modifiers to the given operation.
		Ptr<ASTOp> ApplyMeshModifiers( const Ptr<ASTOp>& sourceOp, const TArray<string>& tags,
			bool bModifiersForBeforeOperations, const void* errorContext);

		// Get the modifiers that have to be applied to elements with a specific tag.
        //void GetSurfacesWithTag(const string& tag, vector<FirstPassGenerator::SURFACE>& surfaces);

        //-----------------------------------------------------------------------------------------
        //!
        Ptr<ASTOp> GenerateTableVariable( TablePtr pTable, const string& strName );

        //!
        Ptr<ASTOp> GenerateMissingBoolCode(const TCHAR* strWhere, bool value, const void* errorContext );

        //!
		template<class NODE_TABLE_PRIVATE, TABLE_COLUMN_TYPE TYPE, OP_TYPE OPTYPE, typename F>
		Ptr<ASTOp> GenerateTableSwitch( const NODE_TABLE_PRIVATE& node, F&& GenerateOption );


    private:

		//! Generate the key with all the relevant state that is used in generation of operations for a node.
		FVisitedKeyMap GetCurrentCacheKey(const NodePtrConst& InNode) const
		{
			FVisitedKeyMap key;
			key.pNode = InNode;
			key.state = m_currentStateIndex;
			if (!m_activeTags.IsEmpty())
			{
				key.activeTags = m_activeTags.Last();
			}
			return key;
		}


		//-----------------------------------------------------------------------------------------
		// Images
		
		/** Options that affect the generation of images. It is like list of what required data we want while parsing down the image node graph. */
		struct FImageGenerationOptions
		{
			/** */
			CompilerOptions::TextureLayoutStrategy ImageLayoutStrategy = CompilerOptions::TextureLayoutStrategy::None;

			/** This is used to introduce additional image generation safety. \TODO: Move this "safety" to optimization? */
			UE::Math::TIntVector2<int32> RectSize;

			/** */
			int32 CurrentStateIndex = -1;

			/** Layout block that we are trying to generate if any. */
			int32 LayoutBlockId = -1;
			Ptr<const Layout> LayoutToApply;

			/** Tags that are active at this point of the generation. */
			TArray<mu::string> ActiveTags;

			friend FORCEINLINE uint32 GetTypeHash(const FImageGenerationOptions& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.ImageLayoutStrategy));
				KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.RectSize));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.CurrentStateIndex));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.LayoutBlockId));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.LayoutToApply.get()));
				return KeyHash;
			}

			FORCEINLINE bool operator==(const FImageGenerationOptions& Other) const
			{
				return ImageLayoutStrategy == Other.ImageLayoutStrategy
					&&
					RectSize == Other.RectSize
					&&
					CurrentStateIndex == Other.CurrentStateIndex
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
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.Node.get()));
				KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.Options));
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
		void GenerateImage_Reference(const FImageGenerationOptions&, FImageGenerationResult&, const NodeImageReference*);
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

		//!
		FImageDesc CalculateImageDesc(const Node::Private&);

		/** Evaluate if the image to generate is big enough to be split in separate operations and tiled afterwards. */
		Ptr<ASTOp> ApplyTiling(Ptr<ASTOp> Source, UE::Math::TIntVector2<int32> Size, EImageFormat Format);

		//!
		Ptr<ASTOp> GenerateImageBlockPatch(Ptr<ASTOp> blockAd, const NodePatchImage* pPatch, Ptr<ASTOp> conditionAd, const FImageGenerationOptions& ImageOptions);

        //-----------------------------------------------------------------------------------------
        // Meshes

		/** Options that affect the generation of meshes. It is like list of what required data we want
		* while parsing down the mesh node graph.
		*/
		struct FMeshGenerationOptions
		{
			/** TODO: Review and document. */
			int32 State = 0;

			/** Tags that are active at this point of the generation. */
			TArray<mu::string> ActiveTags;

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

			friend FORCEINLINE uint32 GetTypeHash(const FMeshGenerationOptions& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.bUniqueVertexIDs));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.bLayouts));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.OverrideLayouts.Num()));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.State));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.ActiveTags.Num()));
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
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.Node.get()));
				KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.Options));
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
		TMap<Ptr<const Layout>, Ptr<const Layout>> m_generatedLayouts;

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
		void PrepareForLayout(LayoutPtrConst GeneratedLayout,
			MeshPtr currentLayoutMesh,
			size_t currentLayoutChannel,
			const void* errorContext,
			const FMeshGenerationOptions& MeshOptions);

		//!
		Ptr<const Layout> AddLayout(Ptr<const Layout> SourceLayout);

		struct FExtensionDataGenerationResult
		{
			Ptr<ASTOp> Op;
		};

		typedef const NodeExtensionData* FGeneratedExtensionDataCacheKey;
		typedef TMap<FGeneratedExtensionDataCacheKey, FExtensionDataGenerationResult> GeneratedExtensionDataMap;
		GeneratedExtensionDataMap m_generatedExtensionData;

		void GenerateExtensionData(FExtensionDataGenerationResult& OutResult, const NodeExtensionDataPtrConst& InUntypedNode);
		void GenerateExtensionData_Constant(FExtensionDataGenerationResult& OutResult, const class NodeExtensionDataConstant* Constant);
		void GenerateExtensionData_Switch(FExtensionDataGenerationResult& OutResult, const class NodeExtensionDataSwitch* Switch);
		void GenerateExtensionData_Variation(FExtensionDataGenerationResult& OutResult, const class NodeExtensionDataVariation* Variation);
		Ptr<ASTOp> GenerateMissingExtensionDataCode(const TCHAR* StrWhere, const void* ErrorContext);

        //-----------------------------------------------------------------------------------------
        // Projectors
        struct FProjectorGenerationResult
        {
            Ptr<ASTOp> op;
            PROJECTOR_TYPE type;
        };

        typedef TMap<FVisitedKeyMap,FProjectorGenerationResult> GeneratedProjectorsMap;
        GeneratedProjectorsMap m_generatedProjectors;

        void GenerateProjector( FProjectorGenerationResult&, const NodeProjectorPtrConst& );
        void GenerateProjector_Constant( FProjectorGenerationResult&, const Ptr<const NodeProjectorConstant>& );
        void GenerateProjector_Parameter( FProjectorGenerationResult&, const Ptr<const NodeProjectorParameter>& );
        void GenerateMissingProjectorCode( FProjectorGenerationResult&, const void* errorContext );

		//-----------------------------------------------------------------------------------------
		// Bools
		struct FBoolGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FVisitedKeyMap, FBoolGenerationResult> GeneratedBoolsMap;
		GeneratedBoolsMap m_generatedBools;

		void GenerateBool(FBoolGenerationResult&, const NodeBoolPtrConst&);
		void GenerateBool_Constant(FBoolGenerationResult&, const Ptr<const NodeBoolConstant>&);
		void GenerateBool_Parameter(FBoolGenerationResult&, const Ptr<const NodeBoolParameter>&);
		void GenerateBool_IsNull(FBoolGenerationResult&, const Ptr<const NodeBoolIsNull>&);
		void GenerateBool_Not(FBoolGenerationResult&, const Ptr<const NodeBoolNot>&);
		void GenerateBool_And(FBoolGenerationResult&, const Ptr<const NodeBoolAnd>&);

		//-----------------------------------------------------------------------------------------
		// Scalars
		struct FScalarGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FVisitedKeyMap, FScalarGenerationResult> GeneratedScalarsMap;
		GeneratedScalarsMap m_generatedScalars;

		void GenerateScalar(FScalarGenerationResult&, const NodeScalarPtrConst&);
		void GenerateScalar_Constant(FScalarGenerationResult&, const Ptr<const NodeScalarConstant>&);
		void GenerateScalar_Parameter(FScalarGenerationResult&, const Ptr<const NodeScalarParameter>&);
		void GenerateScalar_Switch(FScalarGenerationResult&, const Ptr<const NodeScalarSwitch>&);
		void GenerateScalar_EnumParameter(FScalarGenerationResult&, const Ptr<const NodeScalarEnumParameter>&);
		void GenerateScalar_Curve(FScalarGenerationResult&, const Ptr<const NodeScalarCurve>&);
		void GenerateScalar_Arithmetic(FScalarGenerationResult&, const Ptr<const NodeScalarArithmeticOperation>&);
		void GenerateScalar_Variation(FScalarGenerationResult&, const Ptr<const NodeScalarVariation>&);
		void GenerateScalar_Table(FScalarGenerationResult&, const Ptr<const NodeScalarTable>&);
		Ptr<ASTOp> GenerateMissingScalarCode(const TCHAR* strWhere, float value, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Colors
		struct FColorGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FVisitedKeyMap, FColorGenerationResult> GeneratedColorsMap;
		GeneratedColorsMap m_generatedColors;

		void GenerateColor(FColorGenerationResult&, const NodeColourPtrConst&);
		void GenerateColor_Constant(FColorGenerationResult&, const Ptr<const NodeColourConstant>&);
		void GenerateColor_Parameter(FColorGenerationResult&, const Ptr<const NodeColourParameter>&);
		void GenerateColor_Switch(FColorGenerationResult&, const Ptr<const NodeColourSwitch>&);
		void GenerateColor_SampleImage(FColorGenerationResult&, const Ptr<const NodeColourSampleImage>&);
		void GenerateColor_FromScalars(FColorGenerationResult&, const Ptr<const NodeColourFromScalars>&);
		void GenerateColor_Arithmetic(FColorGenerationResult&, const Ptr<const NodeColourArithmeticOperation>&);
		void GenerateColor_Variation(FColorGenerationResult&, const Ptr<const NodeColourVariation>&);
		void GenerateColor_Table(FColorGenerationResult&, const Ptr<const NodeColourTable>&);
		Ptr<ASTOp> GenerateMissingColourCode(const TCHAR* strWhere, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Strings
		struct FStringGenerationResult
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<FVisitedKeyMap, FStringGenerationResult> GeneratedStringsMap;
		GeneratedStringsMap m_generatedStrings;

		void GenerateString(FStringGenerationResult&, const NodeStringPtrConst&);
		void GenerateString_Constant(FStringGenerationResult&, const Ptr<const NodeStringConstant>&);
		void GenerateString_Parameter(FStringGenerationResult&, const Ptr<const NodeStringParameter>&);

        //-----------------------------------------------------------------------------------------
        // Ranges
        struct FRangeGenerationResult
        {
            //
            Ptr<ASTOp> sizeOp;

            //
            string rangeName;

            //
            string rangeUID;
        };

        typedef TMap<FVisitedKeyMap,FRangeGenerationResult> GeneratedRangeMap;
        GeneratedRangeMap m_generatedRanges;

        void GenerateRange( FRangeGenerationResult& result, Ptr<const NodeRange> node);


        //-----------------------------------------------------------------------------------------
        struct FSurfaceGenerationResult
        {
            Ptr<ASTOp> surfaceOp;
        };

        void GenerateSurface( FSurfaceGenerationResult& result,
                              NodeSurfaceNewPtrConst node,
                              const TArray<FirstPassGenerator::SURFACE::EDIT>& edits );
    };


    //---------------------------------------------------------------------------------------------
    //! Analyse the code trying to guess the descriptor of the image genereated by the instruction
    //! address.
    //! \param returnBestOption If true, try to resolve ambiguities returning some value.
    //---------------------------------------------------------------------------------------------
    extern FImageDesc GetImageDesc( const FProgram& program, OP::ADDRESS at,
                                    bool returnBestOption = false,
                                    class FGetImageDescContext* context=nullptr );

    //!
    extern void PartialOptimise( Ptr<ASTOp>& op, const CompilerOptions* options );
    

	
    //---------------------------------------------------------------------------------------------
    template<class NODE_TABLE_PRIVATE, TABLE_COLUMN_TYPE TYPE, OP_TYPE OPTYPE, typename F>
    Ptr<ASTOp> CodeGenerator::GenerateTableSwitch
        (
            const NODE_TABLE_PRIVATE& node, F&& GenerateOption
        )
    {
        TablePtr pTable;
        Ptr<ASTOp> variable;

        map< std::pair<TablePtr,string>, std::pair<TablePtr,Ptr<ASTOp>> >::iterator it
                = m_generatedTables.find
                ( std::pair<TablePtr,string>(node.m_pTable,node.m_parameterName) );
        if ( it!=m_generatedTables.end() )
        {
            pTable = it->second.first;
            variable = it->second.second;
        }

        if ( !pTable )
        {
            // Create the table variable expression
            pTable = node.m_pTable;
            variable = GenerateTableVariable( pTable, node.m_parameterName );

            m_generatedTables[ std::pair<TablePtr,string>(node.m_pTable,node.m_parameterName) ] =
                    std::pair<TablePtr,Ptr<ASTOp>>( pTable, variable );
        }

        // Verify that the table column is the right type
        int colIndex = pTable->FindColumn( node.m_columnName.c_str() );
        if ( colIndex<0 )
        {
            m_pErrorLog->GetPrivate()->Add("Table column not found.", ELMT_ERROR, node.m_errorContext);
            return nullptr;
        }

        if ( pTable->GetPrivate()->m_columns[ colIndex ].m_type != TYPE )
        {
            m_pErrorLog->GetPrivate()->Add("Table column type is not the right type.",
                                           ELMT_ERROR, node.m_errorContext);
            return nullptr;
        }

        // Create the switch to cover all the options
        Ptr<ASTOp> lastSwitch;
        std::size_t rows = pTable->GetPrivate()->m_rows.Num();

        Ptr<ASTOpSwitch> SwitchOp = new ASTOpSwitch();
		SwitchOp->type = OPTYPE;
		SwitchOp->variable = variable;
		SwitchOp->def = nullptr;

		for (size_t i = 0; i < rows; ++i)
        {
            check( pTable->GetPrivate()->m_rows[i].m_id <= 0xFFFF);
            auto condition = (uint16)pTable->GetPrivate()->m_rows[i].m_id;
            Ptr<ASTOp> Branch = GenerateOption( node, colIndex, (int)i, m_pErrorLog.get() );

			if (Branch || TYPE != TCT_MESH)
			{
				SwitchOp->cases.Add(ASTOpSwitch::FCase(condition, SwitchOp, Branch));
			}
        }

        return SwitchOp;
    }
}
