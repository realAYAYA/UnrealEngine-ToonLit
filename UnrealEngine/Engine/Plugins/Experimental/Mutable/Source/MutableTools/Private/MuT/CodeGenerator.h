// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
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
#include "MuT/NodeImageProject.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeObjectState.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeString.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"
#include "MuT/Visitor.h"
#include "Templates/TypeHash.h"

#include <shared_mutex>
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
	class NodeImageDifference;
	class NodeImageFormat;
	class NodeImageGradient;
	class NodeImageInterpolate3;
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
	class NodeImageSelectColour;
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
	class NodeMeshSubtract;
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
	class TaskManager;
	struct OBJECT_STATE;
	struct PROGRAM;



    //---------------------------------------------------------------------------------------------
    //! Code generator
    //---------------------------------------------------------------------------------------------
    class CodeGenerator : public Base,
                          public BaseVisitor,

                          public Visitor<NodeComponentNew::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeComponentEdit::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeLOD::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeObjectNew::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeObjectState::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodeObjectGroup::Private, Ptr<ASTOp>, true>,
                          public Visitor<NodePatchImage::Private, Ptr<ASTOp>, true>
    {
    public:

        CodeGenerator( CompilerOptions::Private* options );

        //! Data will be stored in m_states
        void GenerateRoot( const NodePtrConst pNode, TaskManager* );

	protected:

        Ptr<ASTOp> Generate(const NodePtrConst pNode);

	public:

        Ptr<ASTOp> Visit( const NodeComponentNew::Private& ) override;
        Ptr<ASTOp> Visit( const NodeComponentEdit::Private& ) override;
        Ptr<ASTOp> Visit( const NodeLOD::Private& ) override;
        Ptr<ASTOp> Visit( const NodeObjectNew::Private& ) override;
        Ptr<ASTOp> Visit( const NodeObjectState::Private& ) override;
        Ptr<ASTOp> Visit( const NodeObjectGroup::Private& ) override;
        Ptr<ASTOp> Visit( const NodePatchImage::Private& ) override;

    public:

        //! Settings
        CompilerOptions::Private* m_compilerOptions = nullptr;

		//!
		FirstPassGenerator m_firstPass;


        struct VISITED_MAP_KEY
        {
            VISITED_MAP_KEY()
            {
            }

			friend FORCEINLINE uint32 GetTypeHash(const VISITED_MAP_KEY& InKey)
			{
				uint32 KeyHash = 0;
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.pNode.get()));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageSize[0]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageSize[1]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageRect.min[0]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageRect.min[1]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageRect.size[0]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.imageRect.size[1]));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash(InKey.state));
				KeyHash = HashCombine(KeyHash, ::GetTypeHash((uint64)InKey.activeTags.Num()));
				return KeyHash;
			}

			bool operator==(const VISITED_MAP_KEY& InKey) const
			{
				if (pNode != InKey.pNode) return false;
				if (state != InKey.state) return false;
				if (imageSize != InKey.imageSize) return false;
				if (imageRect.min != InKey.imageRect.min) return false;
				if (imageRect.size != InKey.imageRect.size) return false;
				if (activeTags != InKey.activeTags) return false;
				if (overrideLayouts != InKey.overrideLayouts) return false;
				return true;
			}

            // This reference has to be the smart pointer to avoid memory aliasing, keeping
            // processed nodes alive.
            NodePtrConst pNode;
            vec2<int> imageSize;
            box< vec2<int> > imageRect;
            int state = -1;
			TArray<mu::string> activeTags;
			TArray<LayoutPtrConst> overrideLayouts;
        };

        //! This struct contains additional state propagated from bottom to top of the object node graph.
        //! It is stored for every visited node, and restored when the cache is used.
        struct BOTTOM_UP_STATE
        {
            //! Generated root address for the node.
            Ptr<ASTOp> m_address;
        };
        BOTTOM_UP_STATE m_currentBottomUpState;

        typedef TMap<VISITED_MAP_KEY,BOTTOM_UP_STATE> VisitedMap;
        VisitedMap m_compiled;

        //!
        ErrorLogPtr m_pErrorLog;

        //! While generating code, this contains the index of the state being generated. This
        //! can only be used with the state data in m_firstPass.
        int m_currentStateIndex = -1;

        //! After the entire code generation this contains the information about all the states
        typedef TArray< std::pair<OBJECT_STATE, Ptr<ASTOp>> > StateList;
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

        //! When generating images, here we have the entire source image size and the rect of the
        //! image that we are generating.
        struct IMAGE_STATE
        {
            vec2<int> m_imageSize;
            box< vec2<int> > m_imageRect;
            int32 m_layoutBlock;
            LayoutPtrConst m_pLayout;
        };
		TArray<IMAGE_STATE> m_imageState;

		// (top-down) Tags that are active when generating nodes.
		TArray< TArray<mu::string> > m_activeTags;

        struct PARENT_KEY
        {
            PARENT_KEY()
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

		TArray< PARENT_KEY > m_currentParents;

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
        void PrepareForLayout( LayoutPtrConst GeneratedLayout,
                                  MeshPtr currentLayoutMesh,
                                  size_t currentLayoutChannel,
                                  const void* errorContext );

        //-----------------------------------------------------------------------------------------
        //!
        Ptr<ASTOp> GenerateTableVariable( TablePtr pTable, const string& strName );

        //!
        Ptr<ASTOp> GenerateMissingBoolCode( const char* strWhere, bool value,
                                         const void* errorContext );

        //!
		template<class NODE_TABLE_PRIVATE, TABLE_COLUMN_TYPE TYPE, OP_TYPE OPTYPE, typename F>
		Ptr<ASTOp> GenerateTableSwitch( const NODE_TABLE_PRIVATE& node, F&& GenerateOption );

        //!
        Ptr<ASTOp> GenerateImageBlockPatch( Ptr<ASTOp> blockAd,
                                             const NodePatchImage* pPatch,
                                             Ptr<ASTOp> conditionAd );


    private:

        std::shared_timed_mutex m_codeAccessMutex;

		//! Generate the key with all the relevant state that is used in generation of operations for a node.
		VISITED_MAP_KEY GetCurrentCacheKey(const NodePtrConst& InNode) const
		{
			VISITED_MAP_KEY key;
			key.pNode = InNode;
			key.state = m_currentStateIndex;
			if (!m_imageState.IsEmpty())
			{
				key.imageSize = m_imageState.Last().m_imageSize;
				key.imageRect = m_imageState.Last().m_imageRect;
			}
			if (!m_activeTags.IsEmpty())
			{
				key.activeTags = m_activeTags.Last();
			}
			return key;
		}


		//-----------------------------------------------------------------------------------------
		// Images
		struct IMAGE_GENERATION_RESULT
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<VISITED_MAP_KEY, IMAGE_GENERATION_RESULT> GeneratedImagesMap;
		GeneratedImagesMap m_generatedImages;

		void GenerateImage(IMAGE_GENERATION_RESULT& result, const NodeImagePtrConst& node);
		void GenerateImage_Constant(IMAGE_GENERATION_RESULT&, const NodeImageConstant*);
		void GenerateImage_Difference(IMAGE_GENERATION_RESULT&, const NodeImageDifference*);
		void GenerateImage_Interpolate(IMAGE_GENERATION_RESULT&, const NodeImageInterpolate*);
		void GenerateImage_Saturate(IMAGE_GENERATION_RESULT&, const NodeImageSaturate*);
		void GenerateImage_Table(IMAGE_GENERATION_RESULT&, const NodeImageTable*);
		void GenerateImage_Swizzle(IMAGE_GENERATION_RESULT&, const NodeImageSwizzle*);
		void GenerateImage_SelectColour(IMAGE_GENERATION_RESULT&, const NodeImageSelectColour*);
		void GenerateImage_ColourMap(IMAGE_GENERATION_RESULT&, const NodeImageColourMap*);
		void GenerateImage_Gradient(IMAGE_GENERATION_RESULT&, const NodeImageGradient*);
		void GenerateImage_Binarise(IMAGE_GENERATION_RESULT&, const NodeImageBinarise*);
		void GenerateImage_Luminance(IMAGE_GENERATION_RESULT&, const NodeImageLuminance*);
		void GenerateImage_Layer(IMAGE_GENERATION_RESULT&, const NodeImageLayer*);
		void GenerateImage_LayerColour(IMAGE_GENERATION_RESULT&, const NodeImageLayerColour*);
		void GenerateImage_Resize(IMAGE_GENERATION_RESULT&, const NodeImageResize*);
		void GenerateImage_PlainColour(IMAGE_GENERATION_RESULT&, const NodeImagePlainColour*);
		void GenerateImage_Interpolate3(IMAGE_GENERATION_RESULT&, const NodeImageInterpolate3*);
		void GenerateImage_Project(IMAGE_GENERATION_RESULT&, const NodeImageProject*);
		void GenerateImage_Mipmap(IMAGE_GENERATION_RESULT&, const NodeImageMipmap*);
		void GenerateImage_Switch(IMAGE_GENERATION_RESULT&, const NodeImageSwitch*);
		void GenerateImage_Conditional(IMAGE_GENERATION_RESULT&, const NodeImageConditional*);
		void GenerateImage_Format(IMAGE_GENERATION_RESULT&, const NodeImageFormat*);
		void GenerateImage_Parameter(IMAGE_GENERATION_RESULT&, const NodeImageParameter*);
		void GenerateImage_MultiLayer(IMAGE_GENERATION_RESULT&, const NodeImageMultiLayer*);
		void GenerateImage_Invert(IMAGE_GENERATION_RESULT&, const NodeImageInvert*);
		void GenerateImage_Variation(IMAGE_GENERATION_RESULT&, const NodeImageVariation*);
		void GenerateImage_NormalComposite(IMAGE_GENERATION_RESULT&, const NodeImageNormalComposite*);
		void GenerateImage_Transform(IMAGE_GENERATION_RESULT&, const NodeImageTransform*);

		//!
		ImagePtr GenerateMissingImage(EImageFormat);

		//!
		Ptr<ASTOp> GenerateMissingImageCode(const char* strWhere, EImageFormat, const void* errorContext);

		//!
		Ptr<ASTOp> GeneratePlainImageCode(const vec3<float>& colour);

		//!
		Ptr<ASTOp> GenerateImageFormat(Ptr<ASTOp>, EImageFormat);

		//!
		Ptr<ASTOp> GenerateImageUncompressed(Ptr<ASTOp>);

		//!
		Ptr<ASTOp> GenerateImageSize(Ptr<ASTOp>, FImageSize);

		//!
		FImageDesc CalculateImageDesc(const Node::Private&);


        //-----------------------------------------------------------------------------------------
        // Meshes

		/** Options that affect the generation of meshes. It is like list of what required data we want
		* while parsing down the mesh node graph.
		*/
		struct FMeshGenerationOptions
		{
			/** TODO: Review and document. */
			int State = 0;
			TArray<mu::string> ActiveTags;

			/** Whatever mesh we reach at the leaves of the graph will need to have unique ids for its vertices.
			* This is used to track mesh removal indices, morph data in other nodes, clothing data, etc.
			*/
			bool bUniqueVertexIDs = false;

			/** The meshes at the leaves will need their own layout block data. */
			bool bLayouts = false;

			/** If this has something the layouts in constant meshes will be ignored, because
			* they are supposed to match some other set of layouts. If the vector is empty, layouts
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

			struct EXTRA_LAYOUTS
			{
				/** Source node layouts to use with these extra mesh. They don't have block ids. */
				TArray<Ptr<const Layout>> GeneratedLayouts;
				Ptr<ASTOp> condition;
				Ptr<ASTOp> meshFragment;
			};
			TArray< EXTRA_LAYOUTS > extraMeshLayouts;
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

		//! Map of layouts found in the code already generated. The map is from the source layout
		//! pointer of the layouts in the node meshes to the cloned and modified layout. 
		//! The cloned layout will have absolute block ids assigned.
		TMap<Ptr<const Layout>, Ptr<const Layout>> m_generatedLayouts;

        void GenerateMesh(const FMeshGenerationOptions&, FMeshGenerationResult& result, const NodeMeshPtrConst& node);
        void GenerateMesh_Constant(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshConstant* );
        void GenerateMesh_Format(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshFormat* );
        void GenerateMesh_Morph(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshMorph* );
        void GenerateMesh_MakeMorph(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshMakeMorph* );
        void GenerateMesh_Fragment(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshFragment* );
        void GenerateMesh_Interpolate(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshInterpolate* );
        void GenerateMesh_Switch(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshSwitch* );
        void GenerateMesh_Subtract(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshSubtract* );
        void GenerateMesh_Transform(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshTransform* );
        void GenerateMesh_ClipMorphPlane(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipMorphPlane* );
        void GenerateMesh_ClipWithMesh(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipWithMesh* );
        void GenerateMesh_ApplyPose(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshApplyPose* );
        void GenerateMesh_Variation(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshVariation* );
		void GenerateMesh_Table(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshTable*);
		void GenerateMesh_GeometryOperation(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshGeometryOperation*);
		void GenerateMesh_Reshape(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshReshape*);
		void GenerateMesh_ClipDeform(const FMeshGenerationOptions&, FMeshGenerationResult&, const NodeMeshClipDeform*);

		//!
		Ptr<const Layout> AddLayout(Ptr<const Layout> SourceLayout);

        //-----------------------------------------------------------------------------------------
        // Projectors
        struct PROJECTOR_GENERATION_RESULT
        {
            Ptr<ASTOp> op;
            PROJECTOR_TYPE type;
        };

        typedef TMap<VISITED_MAP_KEY,PROJECTOR_GENERATION_RESULT> GeneratedProjectorsMap;
        GeneratedProjectorsMap m_generatedProjectors;

        void GenerateProjector( PROJECTOR_GENERATION_RESULT&, const NodeProjectorPtrConst& );
        void GenerateProjector_Constant( PROJECTOR_GENERATION_RESULT&, const Ptr<const NodeProjectorConstant>& );
        void GenerateProjector_Parameter( PROJECTOR_GENERATION_RESULT&, const Ptr<const NodeProjectorParameter>& );
        void GenerateMissingProjectorCode( PROJECTOR_GENERATION_RESULT&, const void* errorContext );

		//-----------------------------------------------------------------------------------------
		// Bools
		struct BOOL_GENERATION_RESULT
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<VISITED_MAP_KEY, BOOL_GENERATION_RESULT> GeneratedBoolsMap;
		GeneratedBoolsMap m_generatedBools;

		void GenerateBool(BOOL_GENERATION_RESULT&, const NodeBoolPtrConst&);
		void GenerateBool_Constant(BOOL_GENERATION_RESULT&, const Ptr<const NodeBoolConstant>&);
		void GenerateBool_Parameter(BOOL_GENERATION_RESULT&, const Ptr<const NodeBoolParameter>&);
		void GenerateBool_IsNull(BOOL_GENERATION_RESULT&, const Ptr<const NodeBoolIsNull>&);
		void GenerateBool_Not(BOOL_GENERATION_RESULT&, const Ptr<const NodeBoolNot>&);
		void GenerateBool_And(BOOL_GENERATION_RESULT&, const Ptr<const NodeBoolAnd>&);

		//-----------------------------------------------------------------------------------------
		// Scalars
		struct SCALAR_GENERATION_RESULT
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<VISITED_MAP_KEY, SCALAR_GENERATION_RESULT> GeneratedScalarsMap;
		GeneratedScalarsMap m_generatedScalars;

		void GenerateScalar(SCALAR_GENERATION_RESULT&, const NodeScalarPtrConst&);
		void GenerateScalar_Constant(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarConstant>&);
		void GenerateScalar_Parameter(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarParameter>&);
		void GenerateScalar_Switch(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarSwitch>&);
		void GenerateScalar_EnumParameter(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarEnumParameter>&);
		void GenerateScalar_Curve(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarCurve>&);
		void GenerateScalar_Arithmetic(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarArithmeticOperation>&);
		void GenerateScalar_Variation(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarVariation>&);
		void GenerateScalar_Table(SCALAR_GENERATION_RESULT&, const Ptr<const NodeScalarTable>&);
		Ptr<ASTOp> GenerateMissingScalarCode(const char* strWhere, float value, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Colors
		struct COLOR_GENERATION_RESULT
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<VISITED_MAP_KEY, COLOR_GENERATION_RESULT> GeneratedColorsMap;
		GeneratedColorsMap m_generatedColors;

		void GenerateColor(COLOR_GENERATION_RESULT&, const NodeColourPtrConst&);
		void GenerateColor_Constant(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourConstant>&);
		void GenerateColor_Parameter(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourParameter>&);
		void GenerateColor_Switch(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourSwitch>&);
		void GenerateColor_SampleImage(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourSampleImage>&);
		void GenerateColor_FromScalars(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourFromScalars>&);
		void GenerateColor_Arithmetic(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourArithmeticOperation>&);
		void GenerateColor_Variation(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourVariation>&);
		void GenerateColor_Table(COLOR_GENERATION_RESULT&, const Ptr<const NodeColourTable>&);
		Ptr<ASTOp> GenerateMissingColourCode(const char* strWhere, const void* errorContext);

		//-----------------------------------------------------------------------------------------
		// Strings
		struct STRING_GENERATION_RESULT
		{
			Ptr<ASTOp> op;
		};

		typedef TMap<VISITED_MAP_KEY, STRING_GENERATION_RESULT> GeneratedStringsMap;
		GeneratedStringsMap m_generatedStrings;

		void GenerateString(STRING_GENERATION_RESULT&, const NodeStringPtrConst&);
		void GenerateString_Constant(STRING_GENERATION_RESULT&, const Ptr<const NodeStringConstant>&);
		void GenerateString_Parameter(STRING_GENERATION_RESULT&, const Ptr<const NodeStringParameter>&);

        //-----------------------------------------------------------------------------------------
        // Ranges
        struct RANGE_GENERATION_RESULT
        {
            //
            Ptr<ASTOp> sizeOp;

            //
            string rangeName;

            //
            string rangeUID;
        };

        typedef TMap<VISITED_MAP_KEY,RANGE_GENERATION_RESULT> GeneratedRangeMap;
        GeneratedRangeMap m_generatedRanges;

        void GenerateRange( RANGE_GENERATION_RESULT& result, Ptr<const NodeRange> node);


        //-----------------------------------------------------------------------------------------
        struct SURFACE_GENERATION_RESULT
        {
            Ptr<ASTOp> surfaceOp;
        };

        void GenerateSurface( SURFACE_GENERATION_RESULT& result,
                              NodeSurfaceNewPtrConst node,
                              const TArray<FirstPassGenerator::SURFACE::EDIT>& edits );

        TaskManager* m_pTaskManager = nullptr;
    };


    //---------------------------------------------------------------------------------------------
    //! Analyse the code trying to guess the descriptor of the image genereated by the instruction
    //! address.
    //! \param returnBestOption If true, try to resolve ambiguities returning some value.
    //---------------------------------------------------------------------------------------------
    extern FImageDesc GetImageDesc( const PROGRAM& program, OP::ADDRESS at,
                                    bool returnBestOption = false,
                                    class GetImageDescContext* context=nullptr );

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
			SwitchOp->cases.Add(ASTOpSwitch::CASE(condition, SwitchOp, Branch ));
        }

        return SwitchOp;
    }
}
