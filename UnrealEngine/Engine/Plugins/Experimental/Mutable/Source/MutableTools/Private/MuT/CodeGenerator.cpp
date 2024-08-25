// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator.h"

#include "Containers/Array.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathUtility.h"
#include "MuR/ImagePrivate.h"
#include "MuR/Layout.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuT/ASTOpAddLOD.h"
#include "MuT/ASTOpAddExtensionData.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImageCrop.h"
#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshClipDeform.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshMaskClipMesh.h"
#include "MuT/ASTOpMeshMaskClipUVMask.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshDifference.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshOptimizeSkinning.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/ASTOpLayoutRemoveBlocks.h"
#include "MuT/ASTOpLayoutFromMesh.h"
#include "MuT/ASTOpLayoutMerge.h"
#include "MuT/ASTOpLayoutPack.h"
#include "MuT/CodeGenerator_SecondPass.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNewPrivate.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageFormatPrivate.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageMipmapPrivate.h"
#include "MuT/NodeImageSwizzlePrivate.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshInterpolate.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodeModifierMeshClipDeformPrivate.h"
#include "MuT/NodeModifierMeshClipMorphPlanePrivate.h"
#include "MuT/NodeModifierMeshClipWithMeshPrivate.h"
#include "MuT/NodeModifierMeshClipWithUVMaskPrivate.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeRangeFromScalar.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceEditPrivate.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/TablePrivate.h"
#include "Trace/Detail/Channel.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	CodeGenerator::CodeGenerator(CompilerOptions::Private* options)
	{
		m_compilerOptions = options;

		// Create the message log
		m_pErrorLog = new ErrorLog;

		// Add the parent at the top of the hierarchy
		m_currentParents.Add(FParentKey());
	}

	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateRoot(const Ptr<const Node> pNode)
	{
		MUTABLE_CPUPROFILER_SCOPE(Generate);

		// First pass
		m_firstPass.Generate(m_pErrorLog, pNode.get(), m_compilerOptions->bIgnoreStates, this);

		// Second pass
		SecondPassGenerator SecondPass(&m_firstPass, m_compilerOptions);
		bool bSuccess = SecondPass.Generate(m_pErrorLog, pNode->GetBasePrivate());
		if (!bSuccess)
		{
			return;
		}

		// Main pass for each state
		{
			MUTABLE_CPUPROFILER_SCOPE(MainPass);

            int32 CurrentStateIndex = 0;
			for (const TPair<FObjectState, const Node::Private*>& s : m_firstPass.m_states)
			{
				MUTABLE_CPUPROFILER_SCOPE(MainPassState);

				FGenericGenerationOptions Options;
				Options.State = CurrentStateIndex;

				Ptr<ASTOp> stateRoot = Generate(pNode, Options);
				m_states.Emplace(s.Key, stateRoot);

				++CurrentStateIndex;
			}
		}
	}


	Ptr<ASTOp> CodeGenerator::Generate(const Ptr<const Node> pNode, const FGenericGenerationOptions& Options )
	{
		if (!pNode)
		{
			return nullptr;
		}

        // Type-specific generation
		if (pNode->GetType()->IsA(NodeScalar::GetStaticType()))
		{
			const NodeScalar* ScalarNode = static_cast<const NodeScalar*>(pNode.get());
			FScalarGenerationResult ScalarResult;
			GenerateScalar(ScalarResult, Options, ScalarNode);
			return ScalarResult.op;
		}

		else if (pNode->GetType()->IsA(NodeColour::GetStaticType()))
		{
			const NodeColour* ColorNode = static_cast<const NodeColour*>(pNode.get());
			FColorGenerationResult Result;
			GenerateColor(Result, Options, ColorNode);
			return Result.op;
		}

		else if (pNode->GetType()->IsA(NodeProjector::GetStaticType()))
		{
			const NodeProjector* projNode = static_cast<const NodeProjector*>(pNode.get());
			FProjectorGenerationResult ProjResult;
			GenerateProjector(ProjResult, Options, projNode);
			return ProjResult.op;
		}

		else if (pNode->GetType()->IsA(NodeSurfaceNew::GetStaticType()))
		{
			const NodeSurfaceNew* surfNode = static_cast<const NodeSurfaceNew*>(pNode.get());

			// This happens only if we generate a node graph that has a NodeSurfaceNew at the root.
			FSurfaceGenerationResult surfResult;
			const TArray<FirstPassGenerator::FSurface::FEdit> edits;
			GenerateSurface(surfResult, Options, surfNode, edits);
			return surfResult.surfaceOp;
		}

		else if (pNode->GetType()->IsA(NodeSurfaceVariation::GetStaticType()))
		{
			// This happens only if we generate a node graph that has a NodeSurfaceVariation at the root.
			return nullptr;
		}

		else if (pNode->GetType()->IsA(NodeSurfaceSwitch::GetStaticType()))
		{
			// This happens only if we generate a node graph that has a NodeSurfaceSwitch at the root.
			return nullptr;
		}

		else if (pNode->GetType()->IsA(NodeSurfaceEdit::GetStaticType()))
		{
			// This happens only if we generate a node graph that has a NodeSurfaceEdit at the root.
			return nullptr;
		}

		else if (pNode->GetType()->IsA(NodeModifier::GetStaticType()))
		{
			// This happens only if we generate a node graph that has a modifier at the root.
			return nullptr;
		}


		Ptr<ASTOp> ResultOp;

		// See if it was already generated

		FGeneratedCacheKey Key;
		Key.Node = pNode;
		Key.Options = Options;
		FGeneratedGenericNodesMap::ValueType* it = GeneratedGenericNodes.Find(Key);
		if (it)
		{
			ResultOp = it->op;
		}
		else
		{
			FGenericGenerationResult Result;

			// Generate for each different type of node
			if (pNode->GetType() == NodeComponentNew::GetStaticType())
			{
				Generate_ComponentNew(Options, Result, static_cast<const NodeComponentNew*>(pNode.get()));
			}
			else if (pNode->GetType() == NodeComponentEdit::GetStaticType())
			{
				// Nothing to do: processed elsewhere.
			}
			else if (pNode->GetType() == NodeObjectNew::GetStaticType())
			{
				Generate_ObjectNew(Options, Result, static_cast<const NodeObjectNew*>(pNode.get()));
			}
			else if (pNode->GetType()==NodeObjectGroup::GetStaticType())
			{
				Generate_ObjectGroup(Options, Result, static_cast<const NodeObjectGroup*>(pNode.get()));
			}
			else
			{
				check(false);
			}

			ResultOp = Result.op;
			GeneratedGenericNodes.Add(Key, Result);
		}

		// debug: expensive check of all code generation
//        if (ResultOp)
//        {
//            ASTOpList roots;
//            roots.push_back(ResultOp);
//            ASTOp::FullAssert(roots);
//        }

		return ResultOp;
	}


	void CodeGenerator::GenerateRange(FRangeGenerationResult& Result, const FGenericGenerationOptions& Options, Ptr<const NodeRange> Untyped)
	{
		if (!Untyped)
		{
			Result = FRangeGenerationResult();
			return;
		}

		// See if it was already generated
		FGeneratedCacheKey Key;
		Key.Node = Untyped;
		Key.Options = Options;
		FGeneratedRangeMap::ValueType* it = GeneratedRanges.Find(Key);
		if (it)
		{
			Result = *it;
			return;
		}

		// Generate for each different type of node
		if (Untyped->GetType()==NodeRangeFromScalar::GetStaticType())
		{
			const NodeRangeFromScalar* FromScalar = static_cast<const NodeRangeFromScalar*>(Untyped.get());

			Result = FRangeGenerationResult();
			Result.rangeName = FromScalar->GetName();

			FScalarGenerationResult ChildResult;
			GenerateScalar(ChildResult, Options, FromScalar->GetSize());
			Result.sizeOp = ChildResult.op;
		}
		else
		{
			check(false);
		}


		// Cache the result
		GeneratedRanges.Add(Key, Result);
	}


	Ptr<ASTOp> CodeGenerator::GenerateTableVariable(Ptr<const Node> InNode, const FTableCacheKey& CacheKey, bool bAddNoneOption, const FString& DefaultRowName)
	{
		Ptr<ASTOp> result;

        FParameterDesc param;
        param.m_name = CacheKey.ParameterName;

        if (param.m_name.Len() == 0)
        {
            param.m_name = CacheKey.Table->GetName();
        }
        
		param.m_type = PARAMETER_TYPE::T_INT;
        param.m_defaultValue.Set<ParamIntType>(0);

		// Add the possible values
		{
			// See if there is a string column. If there is one, we will use it as names for the
			// options. Only the first string column will be used.
			int nameCol = -1;
			int32 cols = CacheKey.Table->GetPrivate()->Columns.Num();
			for (int32 c = 0; c < cols && nameCol < 0; ++c)
			{
				if (CacheKey.Table->GetPrivate()->Columns[c].Type == ETableColumnType::String)
				{
					nameCol = c;
				}
			}

			if (bAddNoneOption)
			{
				FParameterDesc::FIntValueDesc nullValue;
				nullValue.m_value = -1;
				nullValue.m_name = "None";
				param.m_possibleValues.Add(nullValue);
				param.m_defaultValue.Set<ParamIntType>(nullValue.m_value);
			}

			// Add every row
			int32 rows = CacheKey.Table->GetPrivate()->Rows.Num();
			for (size_t i = 0; i < rows; ++i)
			{
				FParameterDesc::FIntValueDesc value;
				value.m_value = (int16)CacheKey.Table->GetPrivate()->Rows[i].Id;

				if (nameCol > -1)
				{
					value.m_name = CacheKey.Table->GetPrivate()->Rows[i].Values[nameCol].String;
				}

				param.m_possibleValues.Add(value);

				// Set the first row as the default one (if there is none option)
				if (i == 0 && !bAddNoneOption)
				{
					param.m_defaultValue.Set<ParamIntType>(value.m_value);
				}
				
				// Set the selected row as default (if exists)
				if (value.m_name == DefaultRowName)
				{
					param.m_defaultValue.Set<ParamIntType>(value.m_value);
				}
            }
        }

		Ptr<ASTOpParameter> op = new ASTOpParameter();
		op->type = OP_TYPE::NU_PARAMETER;
		op->parameter = param;

		m_firstPass.ParameterNodes.Add(InNode, op);

		return op;
	}


	Ptr<const Layout> CodeGenerator::AddLayout(Ptr<const Layout> SourceLayout)
	{
		// The layout we are adding must be a source layout, without block ids yet.
		check(SourceLayout->m_blocks.IsEmpty() || SourceLayout->m_blocks[0].m_id == -1);

		Ptr<const Layout>* it = GeneratedLayouts.Find(SourceLayout.get());

		if (it)
		{
			return *it;
		}

		// Assign unique ids to each layout block
		Ptr<Layout> ClonedLayout = SourceLayout->Clone();
		for (int32 b = 0; b < ClonedLayout->m_blocks.Num(); ++b)
		{
			// This is a hard limit due to layout block index data being stored in 16 bit.
			check(m_absoluteLayoutIndex < 65536);
			ClonedLayout->m_blocks[b].m_id = m_absoluteLayoutIndex++;
		}
		check(SourceLayout->m_blocks.Num() == ClonedLayout->m_blocks.Num());
		check(ClonedLayout->m_blocks.IsEmpty() || ClonedLayout->m_blocks[0].m_id != -1);
		GeneratedLayouts.Add(SourceLayout.get(), ClonedLayout);

		return ClonedLayout;
	}


	Ptr<ASTOp> CodeGenerator::GenerateImageBlockPatch(Ptr<ASTOp> blockAd,
		const NodePatchImage* pPatch,
		Ptr<ASTOp> conditionAd,
		const FImageGenerationOptions& ImageOptions )
	{
		// Blend operation
		Ptr<ASTOp> blendAd;
		{
			MUTABLE_CPUPROFILER_SCOPE(PatchBlend);

			Ptr<ASTOpImageLayer> op = new ASTOpImageLayer();
			op->blendType = pPatch->GetPrivate()->m_blendType;
			op->base = blockAd;

			// When we patch from edit nodes, we want to apply it to all the channels.
			// \todo: since we can choose the patch function, maybe we want to be able to
			// select this as well.
			op->Flags = pPatch->GetPrivate()->m_applyToAlpha
				? OP::ImageLayerArgs::F_APPLY_TO_ALPHA
				: 0;

			NodeImage* pImage = pPatch->GetPrivate()->m_pImage.get();
			Ptr<ASTOp> blend;
			if (pImage)
			{
				FImageGenerationResult BlendResult;
				GenerateImage(ImageOptions, BlendResult, pImage);
				blend = BlendResult.op;
			}
			else
			{
				blend = GenerateMissingImageCode(TEXT("Blend top image"), EImageFormat::IF_RGB_UBYTE, pPatch->GetPrivate()->m_errorContext, ImageOptions);
			}
			blend = GenerateImageFormat(blend, blockAd->GetImageDesc().m_format);
			blend = GenerateImageSize(blend, ImageOptions.RectSize);
			op->blend = blend;

			NodeImage* pMask = pPatch->GetPrivate()->m_pMask.get();
			Ptr<ASTOp> mask;
			if (pMask)
			{
				FImageGenerationResult MaskResult;
				GenerateImage(ImageOptions, MaskResult, pMask);
				mask = MaskResult.op;
			}
			else
			{
				// Set the argument default value: affect all pixels.
				// TODO: Special operation code without mask
				FImageGenerationOptions MissingMaskOptions;
				mask = GeneratePlainImageCode(FVector4f(1,1,1,1), MissingMaskOptions);
			}
			mask = GenerateImageFormat(mask, EImageFormat::IF_L_UBYTE);
			mask = GenerateImageSize(mask, ImageOptions.RectSize);
			op->mask = mask;

			blendAd = op;
		}

		// Condition to enable this patch
		if (conditionAd)
		{
			Ptr<ASTOp> conditionalAd;
			{
				Ptr<ASTOpConditional> op = new ASTOpConditional();
				op->type = OP_TYPE::IM_CONDITIONAL;
				op->no = blockAd;
				op->yes = blendAd;
				op->condition = conditionAd;
				conditionalAd = op;
			}

			blockAd = conditionalAd;
		}
		else
		{
			blockAd = blendAd;
		}

		return blockAd;
	}


	void CodeGenerator::Generate_LOD(const FGenericGenerationOptions& Options, FGenericGenerationResult& Result, const NodeLOD* InNode)
	{
		const NodeLOD::Private& node = *InNode->GetPrivate();

		// Build a series of operations to assemble all the LOD components
		Ptr<ASTOp> lastCompOp;

		// Create the expression for each component in this object
		// TODO: More components per operation
		for (int32 t = 0; t < node.m_components.Num(); ++t)
		{
			if (const NodeComponent* pComponentNode = node.m_components[t].get())
			{
				m_currentParents.Last().m_component = (int)t;

				Ptr<ASTOp> componentOp = Generate(pComponentNode, Options);

				if (componentOp)
				{
					check(componentOp->GetOpType() == OP_TYPE::IN_ADDCOMPONENT);

					ASTOpInstanceAdd* typedOp = static_cast<ASTOpInstanceAdd*>(componentOp.get());

					// Complete the instruction adding the base
					typedOp->instance = lastCompOp;

					lastCompOp = componentOp;
				}
			}
		}

		// Add components from child objects
		FAdditionalComponentKey thisKey;
		thisKey.m_lod = m_currentParents.Last().m_lod;
		thisKey.m_pObject = m_currentParents.Last().m_pObject;
		TArray<Ptr<ASTOp>>* addIt = AdditionalComponents.Find(thisKey);
		if (addIt)
		{
			for (const auto& cop : *addIt)
			{
				// Add the additional components after the main ones, this means higher up in the
				// op tree.

                // First find the last op in the chain of IN_ADDCOMPONENT operations
                check( cop->GetOpType() == OP_TYPE::IN_ADDCOMPONENT );
				ASTOpInstanceAdd* typedOp = static_cast<ASTOpInstanceAdd*>(cop.get());
				ASTOpInstanceAdd* bottomOp = typedOp;
				while (bottomOp->instance)
				{
					// Step down
					check(bottomOp->instance->GetOpType() == OP_TYPE::IN_ADDCOMPONENT);
					bottomOp = static_cast<ASTOpInstanceAdd*>(bottomOp->instance.child().get());
				}

				// Chain
				bottomOp->instance = lastCompOp;
				lastCompOp = typedOp;
			}
		}

		// Store for possible parent objects if necessary
		// 2 is because there must be a parent and there is always a null element as well.
		if (lastCompOp && m_currentParents.Num() > 2)
		{
			const auto& parentObjectKey = m_currentParents[m_currentParents.Num() - 2];
			FAdditionalComponentKey parentKey;
			parentKey.m_lod = m_currentParents.Last().m_lod;
			parentKey.m_pObject = parentObjectKey.m_pObject;
			AdditionalComponents.FindOrAdd(parentKey).Add(lastCompOp);
		}

		Result.op = lastCompOp;
	}


	Ptr<ASTOp> CodeGenerator::ApplyTiling(Ptr<ASTOp> Source, UE::Math::TIntVector2<int32> Size, EImageFormat Format)
	{
		// For now always apply tiling
		if (m_compilerOptions->ImageTiling==0)
		{
			return Source;
		}

		int32 TileSize = m_compilerOptions->ImageTiling;

		int32 TilesX = FMath::DivideAndRoundUp<int32>(Size[0], TileSize);
		int32 TilesY = FMath::DivideAndRoundUp<int32>(Size[1], TileSize);
		if (TilesX * TilesY <= 2)
		{
			return Source;
		}

		Ptr<ASTOpFixed> BaseImage = new ASTOpFixed;
		BaseImage->op.type = OP_TYPE::IM_PLAINCOLOUR;
		BaseImage->op.args.ImagePlainColour.size[0] = Size[0];
		BaseImage->op.args.ImagePlainColour.size[1] = Size[1];
		BaseImage->op.args.ImagePlainColour.format = Format;
		BaseImage->op.args.ImagePlainColour.LODs = 1;

		Ptr<ASTOp> CurrentImage = BaseImage;

		for (int32 Y = 0; Y < TilesY; ++Y)
		{
			for (int32 X = 0; X < TilesX; ++X)
			{
				int32 MinX = X * TileSize;
				int32 MinY = Y * TileSize;
				int32 TileSizeX = FMath::Min(TileSize, Size[0] - MinX);
				int32 TileSizeY = FMath::Min(TileSize, Size[1] - MinY);

				Ptr<ASTOpImageCrop> TileImage = new ASTOpImageCrop();
				TileImage->Source = Source;
				TileImage->Min[0] = MinX;
				TileImage->Min[1] = MinY;
				TileImage->Size[0] = TileSizeX;
				TileImage->Size[1] = TileSizeY;

				Ptr<ASTOpImagePatch> PatchedImage = new ASTOpImagePatch();
				PatchedImage->base = CurrentImage;
				PatchedImage->patch = TileImage;
				PatchedImage->location[0] = MinX;
				PatchedImage->location[1] = MinY;

				CurrentImage = PatchedImage;
			}
		}

		return CurrentImage;
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateSurface( FSurfaceGenerationResult& result, 
										 const FGenericGenerationOptions& Options,
                                         Ptr<const NodeSurfaceNew> surfaceNode,
                                         const TArray<FirstPassGenerator::FSurface::FEdit>& edits )
    {
        MUTABLE_CPUPROFILER_SCOPE(GenerateSurface);

        const NodeSurfaceNew::Private& node = *surfaceNode->GetPrivate();

        // Clear the surface generation state
//        m_generatedLayouts.Empty();
//        m_absoluteLayoutIndex = 0;
//        m_currentLayoutMesh = nullptr;
//        m_currentLayoutChannel = 0;
//        m_generatedMeshes.Empty();

        // Build a series of operations to assemble the surface
        Ptr<ASTOp> lastSurfOp;

        // Generate the mesh
        //------------------------------------------------------------------------
        NodeMeshPtr pMesh;
        if ( node.m_meshes.Num()>0 )
        {
            pMesh = node.m_meshes[0].m_pMesh;

            // TODO: Support more meshes
            check( node.m_meshes.Num()==1 );
        }


        FMeshGenerationResult meshResults;


		// We don't add the mesh here, since it will be added directly at the top of the
		// component expression in the NodeComponentNew visitor with the right merges
		// and conditions.
		// But we store it to be used then.

		// Do we need to generate the mesh? Or was it already generated for state conditions 
		// accepting the current state?
		FirstPassGenerator::FSurface* targetSurface = nullptr;
		for (FirstPassGenerator::FSurface& its : m_firstPass.surfaces)
		{
            if (its.node->GetPrivate() == &node)
            {
                // Check state conditions
                bool surfaceValidForThisState =
                    Options.State >= its.stateCondition.Num()
                    ||
                    its.stateCondition[Options.State];

                if (surfaceValidForThisState)
                {
                    if (its.resultSurfaceOp)
                    {
                        // Reuse the entire surface
                        result.surfaceOp = its.resultSurfaceOp;
                        return;
                    }
                    else
                    {
                        // Not already generated, we will generate this
                        targetSurface = &its;
                    }
                }
            }
		}

        if (!targetSurface)
        {
            return;
        }

		// This assumes that the lods are processed in order. It checks it this way because some platforms may have empty lods at the top.
		const bool bIsBaseForSharedSurface = node.SharedSurfaceId != INDEX_NONE && !SharedMeshOptionsMap.Contains(node.SharedSurfaceId);

		// If this is true, we will reuse the surface properties from a higher LOD, se we can skip the generation of material properties and images.
		const bool bShareSurface = node.SharedSurfaceId != INDEX_NONE && !bIsBaseForSharedSurface;

        if ( pMesh )
        {
            MUTABLE_CPUPROFILER_SCOPE(SurfaceMesh);

            Ptr<ASTOp> lastMeshOp;

			// Store the data necessary to apply modifiers for the pre-normal operations stage.
			m_activeTags.Add(node.m_tags);

            // Generate the mesh
			FMeshGenerationOptions MeshOptions;
			MeshOptions.bUniqueVertexIDs = true;
			MeshOptions.bLayouts = true;
			MeshOptions.State = Options.State;
			MeshOptions.ActiveTags = node.m_tags;

			const FMeshGenerationResult* SharedMeshResults = nullptr;
			if (bShareSurface)
			{
				// Do we have the surface we need to share it with?
				SharedMeshResults = SharedMeshOptionsMap.Find(node.SharedSurfaceId);
				check(SharedMeshResults);

				// Override the layouts with the ones from the surface we share
				MeshOptions.OverrideLayouts = SharedMeshResults->GeneratedLayouts;
			}

			// Normalize UVs if we're going to work with images and layouts.
			const bool bNormalizeUVs = node.m_images.Num() && edits.Num();
			MeshOptions.bNormalizeUVs = bNormalizeUVs;

			// Ensure UV islands remain within their main layout block on lower LODs to avoid unexpected reordering 
			// of the layout blocks when reusing a surface between LODs. Used to fix small displacements on vertices
			// that may cause them to fall on a different block.
			MeshOptions.bClampUVIslands = bShareSurface && bNormalizeUVs;

            GenerateMesh(MeshOptions, meshResults, pMesh );
            lastMeshOp = meshResults.meshOp;
            meshResults.extraMeshLayouts.SetNum( edits.Num() );

			m_activeTags.Pop();

            // Let's remember the base mesh of each added mesh, so that we can use them for the
            // "remove mesh" operations that apply to them instead of the base.
            std::map<const Node::Private*,Ptr<ASTOp>> baseMeshesForEachAddedMesh;


            // Apply mesh merges from child objects "edit surface" nodes
            for ( int32 editIndex=0; editIndex<edits.Num(); ++editIndex )
            {
                const FirstPassGenerator::FSurface::FEdit& e = edits[editIndex];

                if ( e.node->m_pMesh )
                {
                    if ( NodeMeshPtr pAdd = e.node->m_pMesh->GetAdd() )
                    {
						// Store the data necessary to apply modifiers for the pre-normal operations stage.
						m_activeTags.Add(e.node->m_tags);
						
						FMeshGenerationOptions MergedMeshOptions;
						MergedMeshOptions.bUniqueVertexIDs = true;
						MergedMeshOptions.bLayouts = true;
						MergedMeshOptions.bClampUVIslands = bShareSurface && bNormalizeUVs;
						MergedMeshOptions.bNormalizeUVs = bNormalizeUVs;
						MergedMeshOptions.State = Options.State;
						MergedMeshOptions.ActiveTags = e.node->m_tags;

						if (SharedMeshResults)
						{
							check(SharedMeshResults->extraMeshLayouts.Num()>editIndex);
							MergedMeshOptions.OverrideLayouts = SharedMeshResults->extraMeshLayouts[editIndex].GeneratedLayouts;
						}

						FMeshGenerationResult addResults;
                        GenerateMesh(MergedMeshOptions, addResults, pAdd);

						m_activeTags.Pop();

                        baseMeshesForEachAddedMesh[e.node] = addResults.baseMeshOp;

						// Apply the modifier for the post-normal operations stage to the added mesh
						bool bModifiersForBeforeOperations = false;
						FGenericGenerationOptions ModifierOptions(Options);
						ModifierOptions.ActiveTags = e.node->m_tags;
						lastMeshOp = ApplyMeshModifiers(Options, lastMeshOp, bModifiersForBeforeOperations, node.m_errorContext);

                        FMeshGenerationResult::FExtraLayouts data;
						data.GeneratedLayouts = addResults.GeneratedLayouts;
						data.condition = e.condition;
                        data.meshFragment = addResults.meshOp;
                        meshResults.extraMeshLayouts[editIndex] = data;

                        Ptr<ASTOpFixed> mop = new ASTOpFixed();
                        mop->op.type = OP_TYPE::ME_MERGE;
                        mop->SetChild(mop->op.args.MeshMerge.base, lastMeshOp );
                        mop->SetChild(mop->op.args.MeshMerge.added, addResults.meshOp );
                        // will merge the meshes under the same surface
                        mop->op.args.MeshMerge.newSurfaceID = 0;

                        // Condition to apply
                        if (e.condition)
                        {
                            Ptr<ASTOpConditional> conditionalAd = new ASTOpConditional();
                            conditionalAd->type = OP_TYPE::ME_CONDITIONAL;
                            conditionalAd->no = lastMeshOp;
                            conditionalAd->yes = mop;
                            conditionalAd->condition = e.condition;
                            lastMeshOp = conditionalAd;
                        }
                        else
                        {
                            lastMeshOp = mop;
                        }
                    }
                }
            }


            // Apply mesh removes from child objects "edit surface" nodes.
            // "Removes" need to come after "Adds" because some removes may refer to added meshes,
            // and not the base.
			// \TODO: Apply base removes first, and then "added meshes" removes here. It may have lower memory footprint during generation.
            Ptr<ASTOpMeshRemoveMask> rop;
            for ( const FirstPassGenerator::FSurface::FEdit& e: edits )
            {
                if ( e.node->m_pMesh )
                {
                    if ( NodeMeshPtr pRemove = e.node->m_pMesh->GetRemove() )
                    {
                        FMeshGenerationResult removeResults;
						FMeshGenerationOptions RemoveMeshOptions;
						RemoveMeshOptions.bUniqueVertexIDs = false;
						RemoveMeshOptions.bLayouts = false;
						RemoveMeshOptions.State = Options.State;
						RemoveMeshOptions.ActiveTags = e.node->m_tags;

                        GenerateMesh(RemoveMeshOptions, removeResults, pRemove );

                        Ptr<ASTOpFixed> maskOp = new ASTOpFixed();
                        maskOp->op.type = OP_TYPE::ME_MASKDIFF;

                        // By default, remove from the base
                        Ptr<ASTOp> removeFrom = meshResults.baseMeshOp;
                        // See if we want to remove from an added mesh instead.
                        if ( e.node->m_pParent )
                        {
                            auto addedBaseMeshIt = baseMeshesForEachAddedMesh.find(e.node->m_pParent->GetBasePrivate());
                            if (addedBaseMeshIt!=baseMeshesForEachAddedMesh.end())
                            {
                                removeFrom = addedBaseMeshIt->second;
                            }
                        }

                        maskOp->SetChild(maskOp->op.args.MeshMaskDiff.source, removeFrom );
                        maskOp->SetChild(maskOp->op.args.MeshMaskDiff.fragment, removeResults.meshOp );

                        if (!rop)
                        {
                            rop = new ASTOpMeshRemoveMask();
                            rop->source = lastMeshOp;
                        }

                        rop->AddRemove( e.condition, maskOp );
                    }
                }
            }

            if (rop)
            {
                lastMeshOp = rop;
            }

            // Apply mesh morphs from child objects "edit surface" nodes
            for ( const FirstPassGenerator::FSurface::FEdit& e: edits )
            {
                if ( NodeMeshPtr pMorph = e.node->m_pMorph )
                {
					// Not needed because it has been generated before already.
					// Base mesh
					//FMeshGenerationResult baseMesh;
					//GenerateMesh(baseMesh, pMesh);

                    // Target mesh
					FMeshGenerationOptions MorphTargetMeshOptions;
					MorphTargetMeshOptions.bUniqueVertexIDs = false;
					MorphTargetMeshOptions.bLayouts = false;
					MorphTargetMeshOptions.State = Options.State;

                    FMeshGenerationResult morphResult;
                    GenerateMesh(MorphTargetMeshOptions, morphResult, pMorph );

					// Morph generation through mesh diff
					Ptr<ASTOp> targetAd = morphResult.meshOp;
                    Ptr<ASTOpMeshDifference> diffAd;
                    {
                        Ptr<ASTOpMeshDifference> op = new ASTOpMeshDifference();
                        op->Base = meshResults.baseMeshOp;
                        op->Target = targetAd;
					
                        // Morphing tex coords here is not supported:
                        // Generating the homogoneous UVs is difficult since we don't have the base
                        // layout yet.                       
                        op->bIgnoreTextureCoords = true;
                        diffAd = op;
                    }

                    // Morph operation
                    Ptr<ASTOp> morphAd;
                    {
                        Ptr<ASTOpMeshMorph> op = new ASTOpMeshMorph();

						// Factor
						if (e.node->m_pFactor)
						{
							FScalarGenerationResult ChildResult;
							GenerateScalar(ChildResult, Options, e.node->m_pFactor);
							op->Factor = ChildResult.op;
						}
						else
						{
							NodeScalarConstantPtr auxNode = new NodeScalarConstant();
							auxNode->SetValue(1.0f);

							FScalarGenerationResult ChildResult;
							GenerateScalar(ChildResult, Options, auxNode);
							op->Factor = ChildResult.op;
						}

						// Base		
						op->Base = lastMeshOp;

						// Targets
						op->Target = diffAd;
                        morphAd = op;
                    }

                    // Condition to apply the morph
                    if (e.condition)
                    {
                        Ptr<ASTOpConditional> conditionalAd = new ASTOpConditional();
                        conditionalAd->type = OP_TYPE::ME_CONDITIONAL;
                        conditionalAd->no = lastMeshOp;
                        conditionalAd->yes = morphAd;
                        conditionalAd->condition = e.condition;
                        lastMeshOp = conditionalAd;
                    }
                    else
                    {
                        lastMeshOp = morphAd;
                    }
                }
            }

			// Apply the modifier for the post-normal operations stage.
			bool bModifiersForBeforeOperations = false;
			FGenericGenerationOptions ModifierOptions(Options);
			ModifierOptions.ActiveTags = node.m_tags;
			lastMeshOp = ApplyMeshModifiers(Options, lastMeshOp, bModifiersForBeforeOperations, node.m_errorContext);

            // Layouts
            for ( int32 LayoutIndex=0; LayoutIndex <meshResults.GeneratedLayouts.Num(); ++LayoutIndex)
            {
                Ptr<ASTOp> layoutOp;

				Ptr<const Layout> pLayout = meshResults.GeneratedLayouts[LayoutIndex].get();
                if ( pLayout )
                {
					if (SharedMeshResults)
					{
						check(SharedMeshResults->layoutOps.Num()>LayoutIndex);
						layoutOp = SharedMeshResults->layoutOps[LayoutIndex];
					}
					else
					{
						// Create a new layout expression

						// Constant layout to start with
						{
							Ptr<ASTOpConstantResource> op = new ASTOpConstantResource();
							op->Type = OP_TYPE::LA_CONSTANT;

							op->SetValue(pLayout, m_compilerOptions->OptimisationOptions.DiskCacheContext);
							layoutOp = op;
						}

						// Add children merged meshes layouts
						for (const FMeshGenerationResult::FExtraLayouts& data : meshResults.extraMeshLayouts)
						{
							if (!data.meshFragment)
							{
								// No mesh to add, we assume there are no layouts to add either.
								check(data.GeneratedLayouts.IsEmpty());
								continue;
							}

							if (data.GeneratedLayouts.Num() != meshResults.GeneratedLayouts.Num())
							{
								m_pErrorLog->GetPrivate()->Add(TEXT("Merged layout has been ignored because the number of layouts is different."), ELMT_ERROR, node.m_errorContext);
							}
							else
							{
								// Constant layout to start with
								Ptr<ASTOp> layoutFragmentAd;
								{
									Ptr<ASTOpConstantResource> op = new ASTOpConstantResource();
									op->Type = OP_TYPE::LA_CONSTANT;

									Ptr<const Layout> pCloned = data.GeneratedLayouts[LayoutIndex];
									op->SetValue(pCloned, m_compilerOptions->OptimisationOptions.DiskCacheContext);

									layoutFragmentAd = op;
								}

								// Merge operation
								Ptr<ASTOpLayoutMerge> mergeAd = new ASTOpLayoutMerge();
								mergeAd->Base = layoutOp;
								mergeAd->Added = layoutFragmentAd;

								// Condition to apply
								if (data.condition)
								{
									Ptr<ASTOpConditional> conditionalAd = new ASTOpConditional();
									conditionalAd->type = OP_TYPE::LA_CONDITIONAL;
									conditionalAd->no = layoutOp;
									conditionalAd->yes = mergeAd;
									conditionalAd->condition = data.condition;
									layoutOp = conditionalAd;
								}
								else
								{
									layoutOp = mergeAd;
								}
							}
						}
					}

					bool bIsOverlayLayout = pLayout->GetLayoutPackingStrategy() == mu::EPackStrategy::OVERLAY_LAYOUT;

                    if (!bIsOverlayLayout && layoutOp)
                    {
                        // Add layout packing instructions
						if (!SharedMeshResults)
						{
                            // Make sure we removed unnecessary blocks
                            Ptr<ASTOpLayoutFromMesh> ExtractOp = new ASTOpLayoutFromMesh();
							ExtractOp->Mesh = lastMeshOp;
							check(LayoutIndex<256);
							ExtractOp->LayoutIndex = uint8(LayoutIndex);

							Ptr<ASTOpLayoutRemoveBlocks> RemoveOp = new ASTOpLayoutRemoveBlocks();
							RemoveOp->Source = layoutOp;
							RemoveOp->ReferenceLayout = ExtractOp;
							layoutOp = RemoveOp;

                            // Pack uv blocks
                            Ptr<ASTOpLayoutPack> op = new ASTOpLayoutPack();
                            op->Source = layoutOp;
                            layoutOp = op;
                        }

                        // Create the expression to apply the layout to the mesh
                        {
                            Ptr<ASTOpFixed> op = new ASTOpFixed();
                            op->op.type = OP_TYPE::ME_APPLYLAYOUT;
                            op->SetChild(op->op.args.MeshApplyLayout.mesh, lastMeshOp );
                            op->SetChild(op->op.args.MeshApplyLayout.layout, layoutOp );
                            op->op.args.MeshApplyLayout.channel = (uint16)LayoutIndex;
                            lastMeshOp = op;
                        }
                    }

                }

                meshResults.layoutOps.Add( layoutOp );
            }

            // Store in the surface for later use.
            targetSurface->resultMeshOp = lastMeshOp;
        }


        // Create the expression for each texture, if we are not reusing the surface from another LOD.
        //------------------------------------------------------------------------
		if (!bShareSurface)
		{
			for (int32 t = 0; t < node.m_images.Num(); ++t)
			{
				MUTABLE_CPUPROFILER_SCOPE(SurfaceTexture);

				// Any image-specific format or mipmapping needs to be applied at the end
				Ptr<NodeImageMipmap> mipmapNode;
				Ptr<NodeImageFormat> formatNode;
				Ptr<NodeImageSwizzle> swizzleNode;

				bool bFound = false;
				NodeImagePtr pImageNode = node.m_images[t].m_pImage;

				while (!bFound && pImageNode)
				{
					if (pImageNode->GetType()==NodeImageMipmap::GetStaticType())
					{
						NodeImageMipmap* tm = static_cast<NodeImageMipmap*>(pImageNode.get());
						if (!mipmapNode) mipmapNode = tm;
						pImageNode = tm->GetSource();
					}
					else if (pImageNode->GetType() == NodeImageFormat::GetStaticType())
					{
						NodeImageFormat* tf = static_cast<NodeImageFormat*>(pImageNode.get());
						if (!formatNode) formatNode = tf;
						pImageNode = tf->GetSource();
					}
					else if (pImageNode->GetType() == NodeImageSwizzle::GetStaticType())
					{
						NodeImageSwizzle* ts = static_cast<NodeImageSwizzle*>(pImageNode.get());
						NodeImage* Source = ts->GetSource(0).get();
						if (!swizzleNode && Source ==ts->GetSource(1) && Source==ts->GetSource(2) && Source==ts->GetSource(3))
						{
							swizzleNode = ts;
							pImageNode = Source;
						}
						else
						{
							bFound = true;
						}
					}
					else
					{
						bFound = true;
					}
				}

				if (bFound)
				{

					const int LayoutIndex = node.m_images[t].m_layoutIndex;

					// If the layout index has been set to negative, it means we should ignore the layout for this image.
					CompilerOptions::TextureLayoutStrategy ImageLayoutStrategy = (LayoutIndex < 0)
						? CompilerOptions::TextureLayoutStrategy::None
						: CompilerOptions::TextureLayoutStrategy::Pack
						;

					if (ImageLayoutStrategy == CompilerOptions::TextureLayoutStrategy::None)
					{
						//check(desc.m_format != EImageFormat::IF_NONE);

						// Generate the image
						FImageGenerationOptions ImageOptions;
						ImageOptions.State = Options.State;
						ImageOptions.ImageLayoutStrategy = ImageLayoutStrategy;
						ImageOptions.ActiveTags = node.m_tags;
						ImageOptions.RectSize = { 0, 0 };
						FImageGenerationResult Result;
						GenerateImage(ImageOptions, Result, pImageNode);
						Ptr<ASTOp> imageAd = Result.op;

						// Look for patches to this block
						for (int32 editIndex = 0; editIndex < edits.Num(); ++editIndex)
						{
							const FirstPassGenerator::FSurface::FEdit& e = edits[editIndex];
							if (t < e.node->m_textures.Num())
							{
								if (const NodePatchImage* pPatch = e.node->m_textures[t].m_pPatch.get())
								{
									imageAd = GenerateImageBlockPatch(imageAd, pPatch, e.condition, ImageOptions);
								}
							}

						}

						check(imageAd);

						if (swizzleNode)
						{
							Ptr<ASTOpImageSwizzle> fop = new ASTOpImageSwizzle();
							fop->Format = swizzleNode->GetPrivate()->m_format;
							fop->Sources[0] = imageAd;
							fop->Sources[1] = imageAd;
							fop->Sources[2] = imageAd;
							fop->Sources[3] = imageAd;
							fop->SourceChannels[0] = swizzleNode->GetPrivate()->m_sourceChannels[0];
							fop->SourceChannels[1] = swizzleNode->GetPrivate()->m_sourceChannels[1];
							fop->SourceChannels[2] = swizzleNode->GetPrivate()->m_sourceChannels[2];
							fop->SourceChannels[3] = swizzleNode->GetPrivate()->m_sourceChannels[3];
							check(fop->Format != EImageFormat::IF_NONE);
							imageAd = fop;
						}

						if (mipmapNode)
						{
							Ptr<ASTOpImageMipmap> op = new ASTOpImageMipmap();
							op->Levels = 0;
							op->Source = imageAd;
							op->BlockLevels = 0;

							op->AddressMode = mipmapNode->GetPrivate()->m_settings.m_addressMode;
							op->FilterType = mipmapNode->GetPrivate()->m_settings.m_filterType;
							op->SharpenFactor = mipmapNode->GetPrivate()->m_settings.m_sharpenFactor;
							op->DitherMipmapAlpha = mipmapNode->GetPrivate()->m_settings.m_ditherMipmapAlpha;
							imageAd = op;
						}

						if (formatNode)
						{
							Ptr<ASTOpImagePixelFormat> fop = new ASTOpImagePixelFormat();
							fop->Format = formatNode->GetPrivate()->m_format;
							fop->FormatIfAlpha = formatNode->GetPrivate()->m_formatIfAlpha;
							fop->Source = imageAd;
							check(fop->Format != EImageFormat::IF_NONE);
							imageAd = fop;
						}

						Ptr<ASTOpInstanceAdd> op = new ASTOpInstanceAdd();
						op->type = OP_TYPE::IN_ADDIMAGE;
						op->instance = lastSurfOp;
						op->value = imageAd;
						op->name = node.m_images[t].m_name;

						lastSurfOp = op;
					}

					else if (ImageLayoutStrategy == CompilerOptions::TextureLayoutStrategy::Pack) //-V547
					{						
						if (LayoutIndex >= meshResults.GeneratedLayouts.Num() ||
							LayoutIndex >= meshResults.layoutOps.Num())
						{
							m_pErrorLog->GetPrivate()->Add("Missing layout in object, or its parent.", ELMT_ERROR, node.m_errorContext);
						}
						else
						{
							const Layout* pLayout = meshResults.GeneratedLayouts[LayoutIndex].get();
							check(pLayout);

							Ptr<ASTOpInstanceAdd> op = new ASTOpInstanceAdd();
							op->type = OP_TYPE::IN_ADDIMAGE;
							op->instance = lastSurfOp;

							// Image
							//-------------------------------------

							// Size of a layout block in pixels
							FIntPoint grid = pLayout->GetGridSize();

							EImageFormat FinalFormat = EImageFormat::IF_NONE;
							int32 BlockPixelsX = 0;
							int32 BlockPixelsY = 0;
							bool bBlocksHaveMips = false;

							bool bImageSizeWarning = false;

							// Start with a blank image. It will be completed later with the blockSize, format and mips information
							Ptr<ASTOpFixed> BlankImageOp;
							Ptr<ASTOp> imageAd;
							{
								BlankImageOp = new ASTOpFixed();
								BlankImageOp->op.type = OP_TYPE::IM_BLANKLAYOUT;
								BlankImageOp->SetChild(BlankImageOp->op.args.ImageBlankLayout.layout, meshResults.layoutOps[LayoutIndex]);
								// The rest ok the op will be completed below
								BlankImageOp->op.args.ImageBlankLayout.mipmapCount = 0;
								imageAd = BlankImageOp;
							}

							auto UpdateBlockSize = [&BlockPixelsX, &BlockPixelsY, &FinalFormat, &bBlocksHaveMips, &bImageSizeWarning, &formatNode, &BlankImageOp, &node, &t, this]( FImageDesc BlockDesc, UE::Math::TIntVector2<uint16> LayoutCellSize )
							{
								if (BlockPixelsX == 0)
								{
									if (!bImageSizeWarning)
									{
										// If the block pixels is not a multiple of the block layout cells
										if ((BlockDesc.m_size[0] % LayoutCellSize[0] != 0) || (BlockDesc.m_size[1] % LayoutCellSize[1] != 0))
										{
											bImageSizeWarning = true;

											int currentLOD = m_currentParents.Last().m_lod;
											FString Msg = FString::Printf(TEXT("A texture [%s] for material [%s] parameter [%s] in LOD [%d] has been resized because it didn't fit the layout. "),
												*node.m_images[t].m_name,
												*node.m_images[t].m_materialName,
												*node.m_images[t].m_materialParameterName,
												currentLOD);
											m_pErrorLog->GetPrivate()->Add(Msg, ELMT_INFO, node.m_errorContext);
										}
									}

									BlockPixelsX = FMath::Max(1, BlockDesc.m_size[0] / LayoutCellSize[0]);
									BlockPixelsY = FMath::Max(1, BlockDesc.m_size[1] / LayoutCellSize[1]);
									bBlocksHaveMips = BlockDesc.m_lods > 1;

									FinalFormat = BlockDesc.m_format;
									if (formatNode)
									{
										FinalFormat = formatNode->GetPrivate()->m_formatIfAlpha;
										if (FinalFormat == EImageFormat::IF_NONE)
										{
											FinalFormat = formatNode->GetPrivate()->m_format;
										}
									}

									// Complete the base op
									BlankImageOp->op.args.ImageBlankLayout.blockSize[0] = uint16(BlockPixelsX);
									BlankImageOp->op.args.ImageBlankLayout.blockSize[1] = uint16(BlockPixelsY);
									BlankImageOp->op.args.ImageBlankLayout.format = GetUncompressedFormat(FinalFormat);
									BlankImageOp->op.args.ImageBlankLayout.generateMipmaps = bBlocksHaveMips;
									BlankImageOp->op.args.ImageBlankLayout.mipmapCount = 0;
								}
							};

							for (int32 BlockIndex = 0; BlockIndex < pLayout->GetBlockCount(); ++BlockIndex)
							{
								// Generate the image
								FImageGenerationOptions ImageOptions;
								ImageOptions.State = Options.State;
								ImageOptions.ImageLayoutStrategy = ImageLayoutStrategy;
								ImageOptions.RectSize = { 0,0 };
								ImageOptions.ActiveTags = node.m_tags;
								ImageOptions.LayoutToApply = pLayout;
								ImageOptions.LayoutBlockId = pLayout->m_blocks[BlockIndex].m_id;
								FImageGenerationResult Result;
								GenerateImage(ImageOptions, Result, pImageNode);
								Ptr<ASTOp> blockAd = Result.op;

								if (!blockAd)
								{
									// The GenerateImage(...) above has failed, skip this block
									result.surfaceOp = nullptr;
									continue;
								}

								// Calculate the desc of the generated block.
								constexpr bool bReturnBestOption = true;
								FImageDesc BlockDesc = blockAd->GetImageDesc(bReturnBestOption, nullptr);

								// Block in layout grid units (cells)
								box< UE::Math::TIntVector2<uint16> > rectInCells;
								pLayout->GetBlock
								(
									BlockIndex,
									&rectInCells.min[0], &rectInCells.min[1],
									&rectInCells.size[0], &rectInCells.size[1]
								);

								// If we don't know the size of a layout block in pixels, calculate it
								UpdateBlockSize(BlockDesc, rectInCells.size);

								// Even if we force the size afterwards, we need some size hint in some cases, like image projections.
								ImageOptions.RectSize = UE::Math::TIntVector2<int32>(BlockDesc.m_size);

								// Look for patches to this block
								for (int32 editIndex = 0; editIndex < edits.Num(); ++editIndex)
								{
									const FirstPassGenerator::FSurface::FEdit& e = edits[editIndex];
									if (t < e.node->m_textures.Num())
									{
										if (const NodePatchImage* pPatch = e.node->m_textures[t].m_pPatch.get())
										{
											// Is the current block to be patched?
											if (pPatch->GetPrivate()->m_blocks.Contains(BlockIndex))
											{
												blockAd = GenerateImageBlockPatch(blockAd, pPatch, e.condition, ImageOptions);
											}
										}
									}
								}

								// Enforce block size and optimizations
								blockAd = GenerateImageSize(blockAd, FIntVector2(BlockDesc.m_size));

								EImageFormat baseFormat = imageAd->GetImageDesc().m_format;
								// Actually don't do it, it will be propagated from the top format operation.
								//Ptr<ASTOp> blockAd = GenerateImageFormat(blockAd, baseFormat);

								// Apply tiling to avoid generating chunks of image that are too big.
								blockAd = ApplyTiling(blockAd, ImageOptions.RectSize, FinalFormat);

								// Compose layout operation
								Ptr<ASTOpImageCompose> composeOp = new ASTOpImageCompose();
								composeOp->Layout = meshResults.layoutOps[LayoutIndex];
								composeOp->Base = imageAd;
								composeOp->BlockImage = blockAd;

								// Set the absolute block index.
								check(pLayout->m_blocks[BlockIndex].m_id >= 0);
								composeOp->BlockIndex = pLayout->m_blocks[BlockIndex].m_id;

								imageAd = composeOp;
							}
							check(imageAd);

							// Apply composition of blocks coming from child objects
							for (int32 editIndex = 0; editIndex < edits.Num(); ++editIndex)
							{
								const FirstPassGenerator::FSurface::FEdit& e = edits[editIndex];
								if (t < e.node->m_textures.Num())
								{
									Ptr<NodeImage> pExtend = e.node->m_textures[t].m_pExtend;
									if (pExtend)
									{
										if (LayoutIndex >= meshResults.extraMeshLayouts[editIndex].GeneratedLayouts.Num()
											||
											!meshResults.extraMeshLayouts[editIndex].GeneratedLayouts[LayoutIndex])
										{
											FString Msg = FString::Printf(TEXT("Trying to extend a layout that doesn't exist in object [%s]."),
												*m_currentParents.Last().m_pObject->m_name
											);

											m_pErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, node.m_errorContext);
										}
										else
										{
											Ptr<const Layout> pExtendLayout = meshResults.extraMeshLayouts[editIndex].GeneratedLayouts[LayoutIndex];

											// Size of a layout block in pixels
											FIntPoint extlayout = pExtendLayout->GetGridSize();

											Ptr<ASTOp> lastBase = imageAd;

											for (int b = 0; b < pExtendLayout->GetBlockCount(); ++b)
											{
												// Generate the image block
												FImageGenerationOptions ImageOptions;
												ImageOptions.State = Options.State;
												ImageOptions.ImageLayoutStrategy = ImageLayoutStrategy;
												ImageOptions.ActiveTags = node.m_tags;
												ImageOptions.RectSize = { 0,0 };
												ImageOptions.LayoutToApply = pExtendLayout;
												ImageOptions.LayoutBlockId = pExtendLayout->m_blocks[b].m_id;
												FImageGenerationResult ExtendResult;
												GenerateImage(ImageOptions, ExtendResult, pExtend);
												Ptr<ASTOp> fragmentAd = ExtendResult.op;

												// Block in layout grid units
												box< UE::Math::TIntVector2<uint16> > rectInCells;
												pExtendLayout->GetBlock
												(
													b,
													&rectInCells.min[0], &rectInCells.min[1],
													&rectInCells.size[0], &rectInCells.size[1]
												);

												FImageDesc ExtendDesc = fragmentAd->GetImageDesc();

												// If we don't know the size of a layout block in pixels, calculate it
												UpdateBlockSize(ExtendDesc, rectInCells.size);

												// Adjust the format and size of the block to be added
												// Actually don't do it, it will be propagated from the top format operation.
												//fragmentAd = GenerateImageFormat(fragmentAd, FinalFormat);

												UE::Math::TIntVector2<int32> expectedSize;
												expectedSize[0] = BlockPixelsX * rectInCells.size[0];
												expectedSize[1] = BlockPixelsY * rectInCells.size[1];
												fragmentAd = GenerateImageSize(fragmentAd, expectedSize);

												// Apply tiling to avoid generating chunks of image that are too big.
												fragmentAd = ApplyTiling(fragmentAd, expectedSize, FinalFormat);

												// Compose operation
												Ptr<ASTOpImageCompose> composeOp = new ASTOpImageCompose();
												composeOp->Layout = meshResults.layoutOps[LayoutIndex];
												composeOp->Base = lastBase;
												composeOp->BlockImage = fragmentAd;

												// Set the absolute block index.
												check(pExtendLayout->m_blocks[b].m_id >= 0);
												composeOp->BlockIndex = pExtendLayout->m_blocks[b].m_id;

												lastBase = composeOp;
											}

											// Condition to enable this image extension
											if (e.condition)
											{
												Ptr<ASTOp> conditionalAd;
												Ptr<ASTOpConditional> cop = new ASTOpConditional();
												cop->type = OP_TYPE::IM_CONDITIONAL;
												cop->no = imageAd;
												cop->yes = lastBase;
												cop->condition = e.condition;
												conditionalAd = cop;
												imageAd = conditionalAd;
											}
											else
											{
												imageAd = lastBase;
											}

										}
									}
								}
							}

							if (swizzleNode)
							{
								Ptr<ASTOpImageSwizzle> fop = new ASTOpImageSwizzle();
								fop->Format = swizzleNode->GetPrivate()->m_format;
								fop->Sources[0] = imageAd;
								fop->Sources[1] = imageAd;
								fop->Sources[2] = imageAd;
								fop->Sources[3] = imageAd;
								fop->SourceChannels[0] = swizzleNode->GetPrivate()->m_sourceChannels[0];
								fop->SourceChannels[1] = swizzleNode->GetPrivate()->m_sourceChannels[1];
								fop->SourceChannels[2] = swizzleNode->GetPrivate()->m_sourceChannels[2];
								fop->SourceChannels[3] = swizzleNode->GetPrivate()->m_sourceChannels[3];
								check(fop->Format != EImageFormat::IF_NONE);
								imageAd = fop;
							}

							// Apply mipmap and format if necessary, skip if format is IF_NONE (possibly because a block was skipped above)
							if (mipmapNode && FinalFormat != EImageFormat::IF_NONE)
							{
								Ptr<ASTOpImageMipmap> mop = new ASTOpImageMipmap();

								// At the end of the day, we want all the mipmaps. Maybe the code
								// optimiser will split the process later.
								mop->Levels = 0;
								mop->bOnlyTail = false;
								mop->Source = imageAd;

								// We have to avoid mips smaller than the image format block size, so
								// we will devide the layout block by the format block
								const FImageFormatData& finfo = GetImageFormatData(FinalFormat);

								int32 mipsX = FMath::CeilLogTwo(BlockPixelsX / finfo.PixelsPerBlockX);
								int32 mipsY = FMath::CeilLogTwo(BlockPixelsY / finfo.PixelsPerBlockY);
								mop->BlockLevels = (uint8)FMath::Max(mipsX, mipsY);
								
								if (BlockPixelsX < finfo.PixelsPerBlockX || BlockPixelsY < finfo.PixelsPerBlockY)
								{
									// In this case, the mipmap will never be useful for blocks, so we indicate that
									// it should make the mips at the root of the expression.
									mop->bOnlyTail = true;
								}

								mop->AddressMode = mipmapNode->GetPrivate()->m_settings.m_addressMode;
								mop->FilterType = mipmapNode->GetPrivate()->m_settings.m_filterType;
								mop->SharpenFactor = mipmapNode->GetPrivate()->m_settings.m_sharpenFactor;
								mop->DitherMipmapAlpha = mipmapNode->GetPrivate()->m_settings.m_ditherMipmapAlpha;

								imageAd = mop;
							}

							else if (bBlocksHaveMips)
							{
								// If the blocks had mipmaps, we still need to generate them after the compose, to have the full chain
								Ptr<ASTOpImageMipmap> mop = new ASTOpImageMipmap();

								// At the end of the day, we want all the mipmaps. Maybe the code
								// optimiser will split the process later.
								mop->Levels = 0;
								mop->bOnlyTail = false;
								mop->Source = imageAd;

								// We have to avoid mips smaller than the image format block size, so
								// we will devide the layout block by the format block
								const FImageFormatData& finfo = GetImageFormatData(FinalFormat);

								int mipsX = (int)ceilf(logf((float)BlockPixelsX / finfo.PixelsPerBlockX) / logf(2.0f));
								int mipsY = (int)ceilf(logf((float)BlockPixelsY / finfo.PixelsPerBlockY) / logf(2.0f));
								mop->BlockLevels = (uint8_t)FMath::Max(mipsX, mipsY);

								if (BlockPixelsX < finfo.PixelsPerBlockX || BlockPixelsY < finfo.PixelsPerBlockY)
								{
									// In this case, the mipmap will never be useful for blocks, so we indicate that
									// it should make the mips at the root of the expression.
									mop->bOnlyTail = true;
								}

								// Not important for the end of the mip tail?
								mop->AddressMode = EAddressMode::ClampToEdge;
								mop->FilterType = EMipmapFilterType::MFT_SimpleAverage;
								mop->SharpenFactor = 0;
								mop->DitherMipmapAlpha = false;

								imageAd = mop;
							}

							if (formatNode)
							{
								Ptr<ASTOpImagePixelFormat> fop = new ASTOpImagePixelFormat();
								fop->Format = formatNode->GetPrivate()->m_format;
								fop->FormatIfAlpha = formatNode->GetPrivate()->m_formatIfAlpha;
								fop->Source = imageAd;
								check(fop->Format != EImageFormat::IF_NONE);
								imageAd = fop;
							}

							op->value = imageAd;

							// Name
							op->name = node.m_images[t].m_name;

							lastSurfOp = op;
						}
					}

					else
					{
						// Unimplemented texture layout strategy
						check(false);
					}
				}
			}

			// Create the expression for each vector
			//------------------------------------------------------------------------
			for (int32 t = 0; t < node.m_vectors.Num(); ++t)
			{
				//MUTABLE_CPUPROFILER_SCOPE(SurfaceVector);

				if (NodeColourPtr pVectorNode = node.m_vectors[t].m_pVector)
				{
					Ptr<ASTOpInstanceAdd> op = new ASTOpInstanceAdd();
					op->type = OP_TYPE::IN_ADDVECTOR;
					op->instance = lastSurfOp;

					// Vector
					FColorGenerationResult VectorResult;
					GenerateColor(VectorResult, Options, pVectorNode);
					op->value = VectorResult.op;

					// Name
					op->name = node.m_vectors[t].m_name;

					lastSurfOp = op;
				}
			}

			// Create the expression for each scalar
			//------------------------------------------------------------------------
			for (int32 t = 0; t < node.m_scalars.Num(); ++t)
			{
				// MUTABLE_CPUPROFILER_SCOPE(SurfaceScalar);

				if (NodeScalarPtr pScalarNode = node.m_scalars[t].m_pScalar)
				{
					Ptr<ASTOpInstanceAdd> op = new ASTOpInstanceAdd();
					op->type = OP_TYPE::IN_ADDSCALAR;
					op->instance = lastSurfOp;

					// Scalar
					FScalarGenerationResult ScalarResult;
					GenerateScalar(ScalarResult, Options, pScalarNode);
					op->value = ScalarResult.op;

					// Name
					op->name = node.m_scalars[t].m_name;

					lastSurfOp = op;
				}
			}

			// Create the expression for each string
			//------------------------------------------------------------------------
			for (int32 t = 0; t < node.m_strings.Num(); ++t)
			{
				if (NodeStringPtr pStringNode = node.m_strings[t].m_pString)
				{
					Ptr<ASTOpInstanceAdd> op = new ASTOpInstanceAdd();
					op->type = OP_TYPE::IN_ADDSTRING;
					op->instance = lastSurfOp;

					FStringGenerationResult StringResult;
					GenerateString(StringResult, Options, pStringNode);
					op->value = StringResult.op;

					// Name
					op->name = node.m_strings[t].m_name;

					lastSurfOp = op;
				}
			}
		}

        result.surfaceOp = lastSurfOp;
        targetSurface->resultSurfaceOp = lastSurfOp;

		// If we are going to share this surface properties, remember it.
		if (bIsBaseForSharedSurface)
		{
			check(!SharedMeshOptionsMap.Contains(node.SharedSurfaceId));
			SharedMeshOptionsMap.Add( node.SharedSurfaceId, meshResults );
		}
    }


    //---------------------------------------------------------------------------------------------
    //Ptr<ASTOp> CodeGenerator::Visit( const NodeComponentNew::Private& node )
	void CodeGenerator::Generate_ComponentNew(const FGenericGenerationOptions& Options, FGenericGenerationResult& Result, const NodeComponentNew* InNode)
	{
		const NodeComponentNew::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeComponentNew);

		// Build a series of operations to assemble the component
        Ptr<ASTOp> lastCompOp;
        Ptr<ASTOp> lastMeshOp;
        FString lastMeshName;

        // This generates a different ID for each surface. It can be used to match it to the
        // mesh surface, or for debugging. It cannot be 0 because it is a special case for the
        // merge operation.
        int surfaceID=1;

        // Look for all surfaces that belong to this component
		for (int32 i = 0; i<m_firstPass.surfaces.Num(); ++i, ++surfaceID)
		{
			const FirstPassGenerator::FSurface& its = m_firstPass.surfaces[i];
			if (its.component==&node)
			{
                // Apply state conditions: only generate it if it enabled in this state
                {
                    bool enabledInThisState = true;
                    if (its.stateCondition.Num() && Options.State >= 0)
                    {
                        enabledInThisState =
                                (Options.State < its.stateCondition.Num())
                                &&
                                ( its.stateCondition[Options.State] );
                    }
                    if (!enabledInThisState)
                    {
                        continue;
                    }
                }

                Ptr<ASTOpInstanceAdd> sop = new ASTOpInstanceAdd();
                sop->type = OP_TYPE::IN_ADDSURFACE;
                sop->name = its.node->GetPrivate()->m_name;
                sop->instance = lastCompOp;

				FSurfaceGenerationResult surfaceGenerationResult;
                GenerateSurface( surfaceGenerationResult, Options, its.node, its.edits );
                sop->value = surfaceGenerationResult.surfaceOp;

                sop->id = surfaceID;
                sop->ExternalId = its.node->GetPrivate()->ExternalId;
                sop->SharedSurfaceId = its.node->GetPrivate()->SharedSurfaceId;
                Ptr<ASTOp> surfaceAt = sop;

				// TODO: This could be done earlier?
                Ptr<ASTOpFixed> surfCondOp = new ASTOpFixed();
                surfCondOp->op.type = OP_TYPE::BO_AND;
                surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.a, its.objectCondition );
                surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.b, its.surfaceCondition );
                Ptr<ASTOp> surfaceCondition = surfCondOp;

                if (surfaceCondition)
                {
                    Ptr<ASTOpConditional> op = new ASTOpConditional();
                    op->type = OP_TYPE::IN_CONDITIONAL;
                    op->no = lastCompOp;
                    op->yes = surfaceAt;
                    op->condition = surfaceCondition;
                    lastCompOp = op;
                }
                else
                {
                    lastCompOp = surfaceAt;
                }

                // Add the mesh with its condition
                Ptr<ASTOp> mergeAd;
                if (!lastMeshOp && its.node->GetPrivate()->m_meshes.Num())
                {
                    // \todo: mesh index in case of multiple meshes.
                    const int meshIndex = 0;
                    lastMeshName = its.node->GetPrivate()->m_meshes[ meshIndex ].m_name;
                }

                // We add the merge op even for the first mesh, so that we set the surface id.
                {
                    Ptr<ASTOp> added = its.resultMeshOp;
 
                    Ptr<ASTOpFixed> mop = new ASTOpFixed();
                    mop->op.type = OP_TYPE::ME_MERGE;
                    mop->SetChild(mop->op.args.MeshMerge.base, lastMeshOp );
                    mop->SetChild(mop->op.args.MeshMerge.added, added );
                    mop->op.args.MeshMerge.newSurfaceID = surfaceID;
                    mergeAd = mop;
                }

                if (surfaceCondition)
                {
                    Ptr<ASTOpConditional> op = new ASTOpConditional();
                    op->type = OP_TYPE::ME_CONDITIONAL;
                    op->no = lastMeshOp;
                    op->yes = mergeAd;
                    op->condition = surfaceCondition;
                    lastMeshOp = op;
                }
                else
                {
                    lastMeshOp = mergeAd;
                }
			}
		}

		// Add op to optimize the skinning of the resulting mesh
		{
			Ptr<ASTOpMeshOptimizeSkinning> mop = new ASTOpMeshOptimizeSkinning();
			mop->source = lastMeshOp;
			lastMeshOp = mop;
		}

        // Add the component mesh
        {
            Ptr<ASTOpInstanceAdd> iop = new ASTOpInstanceAdd();
            iop->type = OP_TYPE::IN_ADDMESH;
            iop->instance = lastCompOp;
            iop->value = lastMeshOp;

            // Name
            // \todo: do mesh names make any sense?
            iop->name = lastMeshName;

            lastCompOp = iop;
        }


        Ptr<ASTOpInstanceAdd> iop = new ASTOpInstanceAdd();
        iop->type = OP_TYPE::IN_ADDCOMPONENT;
        iop->value = lastCompOp;
        iop->name = node.m_name;
    	iop->id = node.m_id;
        lastCompOp = iop;

        Result.op = lastCompOp;
    }


    //---------------------------------------------------------------------------------------------
   // Ptr<ASTOp> CodeGenerator::Visit( const NodeObjectNew::Private& node )
	void CodeGenerator::Generate_ObjectNew(const FGenericGenerationOptions& Options, FGenericGenerationResult& Result, const NodeObjectNew* InNode)
	{
		const NodeObjectNew::Private& node = *InNode->GetPrivate();
		
		MUTABLE_CPUPROFILER_SCOPE(NodeObjectNew);

        m_currentParents.Add( FParentKey() );
        m_currentParents.Last().m_pObject = &node;

        // Parse the child objects first, which will accumulate operations in the patching lists
        for ( int32 t=0; t<node.m_children.Num(); ++t )
        {
            if ( const NodeObject* pChildNode = node.m_children[t].get() )
            {
                Ptr<ASTOp> paramOp;

                // If there are parent objects, the condition of this object depends on the
                // condition of the parent object
                if ( m_currentObject.Num() )
                {
                    paramOp = m_currentObject.Last().m_condition;
                }
                else
                {
                    // In case there is no group node, we generate a constant true condition
                    // This condition will be overwritten by the group nodes.
                    Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
                    op->value = true;
                    paramOp = op;
                }

				FObjectGenerationData data;
                data.m_condition = paramOp;
                m_currentObject.Add( data );

                // This op is ignored: everything is stored as patches to apply to the parent when
                // it is compiled.
                Generate( pChildNode, Options );

                m_currentObject.Pop();
            }
        }

        // Create the expression for each lod
        Ptr<ASTOpAddLOD> lodsOp = new ASTOpAddLOD();
        for ( int32 t=0; t<node.m_lods.Num(); ++t )
        {
            if ( const NodeLOD* pLODNode = node.m_lods[t].get() )
            {
                m_currentParents.Last().m_lod = t;

				FGenericGenerationResult LODResult;
				Generate_LOD(Options, LODResult, pLODNode);
                lodsOp->lods.Emplace( lodsOp, LODResult.op);
            }
        }
        Ptr<ASTOp> rootOp = lodsOp;

		// Add an ASTOpAddExtensionData for each connected ExtensionData node
		for (const NodeObjectNew::Private::NamedExtensionDataNode& NamedNode : node.m_extensionDataNodes)
		{
			if (!NamedNode.Node.get())
			{
				// No node connected
				continue;
			}

			// Name must be valid
			check(NamedNode.Name.Len() > 0);

			FExtensionDataGenerationResult ChildResult;
			GenerateExtensionData(ChildResult, Options, NamedNode.Node);

			if (!ChildResult.Op.get())
			{
				// Failed to generate anything for this node
				continue;
			}

			FConditionalExtensionDataOp& SavedOp = ConditionalExtensionDataOps.AddDefaulted_GetRef();
			if (m_currentObject.Num() > 0)
			{
				SavedOp.Condition = m_currentObject.Last().m_condition;
			}
			SavedOp.ExtensionDataOp = ChildResult.Op;
			SavedOp.ExtensionDataName = NamedNode.Name;
		}

		if (m_currentObject.Num() == 0)
		{
			for (const FConditionalExtensionDataOp& SavedOp : ConditionalExtensionDataOps)
			{
				Ptr<ASTOpAddExtensionData> ExtensionPinOp = new ASTOpAddExtensionData();
				ExtensionPinOp->Instance = ASTChild(ExtensionPinOp, rootOp);
				ExtensionPinOp->ExtensionData = ASTChild(ExtensionPinOp, SavedOp.ExtensionDataOp);
				ExtensionPinOp->ExtensionDataName = SavedOp.ExtensionDataName;

				if (SavedOp.Condition.get())
				{
					Ptr<ASTOpConditional> ConditionOp = new ASTOpConditional();
					ConditionOp->type = OP_TYPE::IN_CONDITIONAL;
					ConditionOp->no = rootOp;
					ConditionOp->yes = ExtensionPinOp;
					ConditionOp->condition = ASTChild(ConditionOp, SavedOp.Condition);
					
					rootOp = ConditionOp;
				}
				else
				{
					rootOp = ExtensionPinOp;
				}
			}
		}

        m_currentParents.Pop();

        Result.op = rootOp;
    }


    //---------------------------------------------------------------------------------------------
    //Ptr<ASTOp> CodeGenerator::Visit( const NodeObjectGroup::Private& node )
	void CodeGenerator::Generate_ObjectGroup(const FGenericGenerationOptions& Options, FGenericGenerationResult& Result, const NodeObjectGroup* InNode)
	{
		const NodeObjectGroup::Private& node = *InNode->GetPrivate();

		TArray<FString> usedNames;

        // Parse the child objects first, which will accumulate operations in the patching lists
        for ( int32 t=0; t<node.m_children.Num(); ++t )
        {
            if ( const NodeObject* pChildNode = node.m_children[t].get() )
            {
				// Look for the child condition in the first pass
                Ptr<ASTOp> conditionOp;
				bool found = false;
                for( int32 i = 0; !found && i != m_firstPass.objects.Num(); i++ )
				{
					FirstPassGenerator::FObject& it = m_firstPass.objects[i];
					if (it.node == pChildNode->GetBasePrivate())
					{
						found = true;
						conditionOp = it.condition;
					}
				}

                // \todo
                // TrunkStraight_02_BranchTop crash
                // It may happen with partial compilations?
                // check(found);

				FObjectGenerationData data;
                data.m_condition = conditionOp;
                m_currentObject.Add( data );

                // This op is ignored: everything is stored as patches to apply to the parent when
                // it is compiled.
                Generate( pChildNode, Options );

                m_currentObject.Pop();

				// Check for duplicated child names
				FString strChildName = pChildNode->GetName();
				if (usedNames.Contains(strChildName))
				{
					FString Msg = FString::Printf(TEXT("Object group has more than one children with the same name [%s]."), *strChildName );
					m_pErrorLog->GetPrivate()->Add(Msg, ELMT_WARNING, node.m_errorContext);
				}
				else
				{
					usedNames.Add(strChildName);
				}
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GenerateMissingBoolCode(const TCHAR* Where, bool Value, const void* ErrorContext )
    {
        // Log a warning
		FString Msg = FString::Printf(TEXT("Required connection not found: %s"), Where);
        m_pErrorLog->GetPrivate()->Add( Msg, ELMT_ERROR, ErrorContext);

        // Create a constant node
        Ptr<NodeBoolConstant> pNode = new NodeBoolConstant();
        pNode->SetValue(Value);

		FBoolGenerationResult ChildResult;
		FGenericGenerationOptions Options;
        GenerateBool(ChildResult,Options, pNode );
		return ChildResult.op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GetModifiersFor(
		const TArray<FString>& SurfaceTags,
		int32 LOD, bool bModifiersForBeforeOperations,
		TArray<FirstPassGenerator::FModifier>& OutModifiers)
	{
        MUTABLE_CPUPROFILER_SCOPE(GetModifiersFor);

		if (SurfaceTags.IsEmpty())
		{
			return;
		}

		for (const FirstPassGenerator::FModifier& m: m_firstPass.modifiers)
		{
			// Correct LOD?
			if (m.lod != LOD)
			{
				continue;
			}

			// Correct stage?
			if (m.node->bApplyBeforeNormalOperations != bModifiersForBeforeOperations)
			{
				continue;
			}

			// Already there?
			bool bAlreadyAdded = 
				OutModifiers.FindByPredicate( [&m](const FirstPassGenerator::FModifier& c) {return c.node == m.node; })
				!= 
				nullptr;

			if (bAlreadyAdded)
			{
				continue;
			}

			// Matching tags?
			bool bApply = false;

			switch (m.node->MultipleTagsPolicy)
			{
			case EMutableMultipleTagPolicy::OnlyOneRequired:
			{
				for (const FString& Tag: m.node->RequiredTags)
				{
					if (SurfaceTags.Contains(Tag))
					{
						bApply = true;
						break;
					}
				}
				break;
			}

			case EMutableMultipleTagPolicy::AllRequired:
			{
				bApply = true;
				for (const FString& Tag : m.node->RequiredTags)
				{
					if (!SurfaceTags.Contains(Tag))
					{
						bApply = false;
						break;
					}
				}
			}
			}

			if (bApply)
			{
				OutModifiers.Add(m);
			}
		}
	}

	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> CodeGenerator::ApplyMeshModifiers(
		const FGenericGenerationOptions& Options,
		const Ptr<ASTOp>& sourceOp,
		bool bModifiersForBeforeOperations,
		const void* errorContext )
	{
		Ptr<ASTOp> lastMeshOp = sourceOp;

		// Apply mesh modifiers
		TArray<FirstPassGenerator::FModifier> modifiers;

		int currentLOD = m_currentParents.Last().m_lod;
		GetModifiersFor(Options.ActiveTags, currentLOD, bModifiersForBeforeOperations, modifiers);

		Ptr<ASTOp> preModifiersMesh = lastMeshOp;

		m_activeTags.Add({});

		// Process clip-with-mesh modifiers
		Ptr<ASTOpMeshRemoveMask> removeOp;
		for (const FirstPassGenerator::FModifier& m : modifiers)
		{
			if (m.node->m_pNode->GetType()== NodeModifierMeshClipWithMesh::GetStaticType())
			{
				const NodeModifierMeshClipWithMesh::Private* TypedClipNode = static_cast<const NodeModifierMeshClipWithMesh::Private*>(m.node);
				Ptr<ASTOpMeshMaskClipMesh> op = new ASTOpMeshMaskClipMesh();
				op->source = preModifiersMesh;

				// Parameters
				FMeshGenerationOptions ClipOptions;
				ClipOptions.bUniqueVertexIDs = false;
				ClipOptions.bLayouts = false;
				ClipOptions.State = Options.State;

				FMeshGenerationResult clipResult;
				GenerateMesh(ClipOptions, clipResult, TypedClipNode->ClipMesh);
				op->clip = clipResult.meshOp;

				if (!op->clip)
				{
					m_pErrorLog->GetPrivate()->Add("Clip mesh has not been generated", ELMT_ERROR, errorContext);
					continue;
				}

				Ptr<ASTOp> maskAt = op;

				if (!removeOp)
				{
					removeOp = new ASTOpMeshRemoveMask();
					removeOp->source = lastMeshOp;
					lastMeshOp = removeOp;
				}

				Ptr<ASTOpFixed> surfCondOp = new ASTOpFixed();
				surfCondOp->op.type = OP_TYPE::BO_AND;
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.a, m.objectCondition);
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.b, m.surfaceCondition);
				Ptr<ASTOp> fullCondition = surfCondOp;

				removeOp->AddRemove(fullCondition, maskAt);
			}
		}

		// Process clip-with-mask modifiers
		for (const FirstPassGenerator::FModifier& m : modifiers)
		{
			if (m.node->m_pNode->GetType() == NodeModifierMeshClipWithUVMask::GetStaticType())
			{
				const NodeModifierMeshClipWithUVMask::Private* TypedClipNode = static_cast<const NodeModifierMeshClipWithUVMask::Private*>(m.node);
				Ptr<ASTOpMeshMaskClipUVMask> op = new ASTOpMeshMaskClipUVMask();
				op->Source = preModifiersMesh;
				op->LayoutIndex = TypedClipNode->LayoutIndex;

				// Parameters
				FImageGenerationOptions ClipOptions;
				ClipOptions.ImageLayoutStrategy = CompilerOptions::TextureLayoutStrategy::None;
				ClipOptions.LayoutBlockId = -1;
				ClipOptions.State = Options.State;

				FImageGenerationResult ClipMaskResult;
				GenerateImage(ClipOptions, ClipMaskResult, TypedClipNode->ClipMask);

				// It could be IF_L_UBIT, but since this should be optimized out at compile time, leave the most cpu efficient.
				op->Mask = GenerateImageFormat(ClipMaskResult.op, mu::EImageFormat::IF_L_UBYTE);

				if (!op->Mask)
				{
					m_pErrorLog->GetPrivate()->Add("Clip UV mask has not been generated", ELMT_ERROR, errorContext);
					continue;
				}

				Ptr<ASTOp> maskAt = op;

				if (!removeOp)
				{
					removeOp = new ASTOpMeshRemoveMask();
					removeOp->source = lastMeshOp;
					lastMeshOp = removeOp;
				}

				Ptr<ASTOpFixed> surfCondOp = new ASTOpFixed();
				surfCondOp->op.type = OP_TYPE::BO_AND;
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.a, m.objectCondition);
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.b, m.surfaceCondition);
				Ptr<ASTOp> fullCondition = surfCondOp;

				removeOp->AddRemove(fullCondition, maskAt);
			}
		}

		// Process clip-morph-plane modifiers
		for (const FirstPassGenerator::FModifier& m : modifiers)
		{
			Ptr<ASTOp> modifiedMeshOp;

			if (m.node->m_pNode->GetType() == NodeModifierMeshClipMorphPlane::GetStaticType())
			{
				const NodeModifierMeshClipMorphPlane::Private* TypedNode = static_cast<const NodeModifierMeshClipMorphPlane::Private*>(m.node);
				Ptr<ASTOpMeshClipMorphPlane> op = new ASTOpMeshClipMorphPlane();
				op->source = lastMeshOp;

				// Morph to an ellipse
				{
					FShape morphShape;
					morphShape.type = (uint8_t)FShape::Type::Ellipse;
					morphShape.position = TypedNode->m_origin;
					morphShape.up = TypedNode->m_normal;
					// TODO: Move rotation to ellipse rotation reference base instead of passing it directly
					morphShape.size = FVector3f(TypedNode->m_radius1, TypedNode->m_radius2, TypedNode->m_rotation);

					// Generate a "side" vector.
					// \todo: make generic and move to the vector class
					{
						// Generate vector perpendicular to normal for ellipse rotation reference base
						FVector3f aux_base(0.f, 1.f, 0.f);

						if (FMath::Abs(FVector3f::DotProduct(TypedNode->m_normal, aux_base)) > 0.95f)
						{
							aux_base = FVector3f(0.f, 0.f, 1.f);
						}

						morphShape.side = FVector3f::CrossProduct(TypedNode->m_normal, aux_base);
					}
					op->morphShape = morphShape;
				}

				// Selection box
				if (TypedNode->m_vertexSelectionType == NodeModifierMeshClipMorphPlane::Private::VS_SHAPE)
				{
					op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_SHAPE;
					FShape selectionShape;
					selectionShape.type = (uint8_t)FShape::Type::AABox;
					selectionShape.position = TypedNode->m_selectionBoxOrigin;
					selectionShape.size = TypedNode->m_selectionBoxRadius;
					op->selectionShape = selectionShape;
				}
				else if (TypedNode->m_vertexSelectionType == NodeModifierMeshClipMorphPlane::Private::VS_BONE_HIERARCHY)
				{
					op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY;
					op->vertexSelectionBone = TypedNode->m_vertexSelectionBone;
					op->vertexSelectionBoneMaxRadius = TypedNode->m_maxEffectRadius;
				}
				else
				{
					op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_NONE;
				}

				// Parameters
				op->dist = TypedNode->m_dist;
				op->factor = TypedNode->m_factor;

				modifiedMeshOp = op;
			}

			if (modifiedMeshOp)
			{
				Ptr<ASTOpFixed> surfCondOp = new ASTOpFixed();
				surfCondOp->op.type = OP_TYPE::BO_AND;
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.a, m.objectCondition);
				surfCondOp->SetChild(surfCondOp->op.args.BoolBinary.b, m.surfaceCondition);
				Ptr<ASTOp> fullCondition = surfCondOp;

				Ptr<ASTOpConditional> op = new ASTOpConditional();
				op->type = OP_TYPE::ME_CONDITIONAL;
				op->no = lastMeshOp;
				op->yes = modifiedMeshOp;
				op->condition = fullCondition;
				lastMeshOp = op;
			}
		}

    	// Process clip deform modifiers.
		for (const FirstPassGenerator::FModifier& M : modifiers)
		{
			Ptr<ASTOp> ModifiedMeshOp;

			if (M.node->m_pNode->GetType()==NodeModifierMeshClipDeform::GetStaticType())
			{
				const NodeModifierMeshClipDeform::Private* TypedClipNode = static_cast<const NodeModifierMeshClipDeform::Private*>(M.node);
				Ptr<ASTOpMeshBindShape>  BindOp = new ASTOpMeshBindShape();
				Ptr<ASTOpMeshClipDeform> ClipOp = new ASTOpMeshClipDeform();

				FMeshGenerationOptions ClipOptions;
				ClipOptions.bUniqueVertexIDs = false;
				ClipOptions.bLayouts = false;
				ClipOptions.State = Options.State;

				FMeshGenerationResult ClipShapeResult;
				GenerateMesh(ClipOptions, ClipShapeResult, TypedClipNode->ClipMesh);
				ClipOp->ClipShape = ClipShapeResult.meshOp;
				
				BindOp->Mesh = lastMeshOp;
				BindOp->Shape = ClipShapeResult.meshOp; 
				BindOp->BindingMethod = static_cast<uint32>(TypedClipNode->BindingMethod);
	
				ClipOp->Mesh = BindOp;

				if (!ClipOp->ClipShape)
				{
					m_pErrorLog->GetPrivate()->Add
					("Clip shape mesh has not been generated", ELMT_ERROR, errorContext);
				}
				else
				{
					ModifiedMeshOp = ClipOp;
				}
			}
			
			if (ModifiedMeshOp)
			{
				Ptr<ASTOpFixed> SurfCondOp = new ASTOpFixed();
				SurfCondOp->op.type = OP_TYPE::BO_AND;
				SurfCondOp->SetChild(SurfCondOp->op.args.BoolBinary.a, M.objectCondition);
				SurfCondOp->SetChild(SurfCondOp->op.args.BoolBinary.b, M.surfaceCondition);
				Ptr<ASTOp> FullCondition = SurfCondOp;

				Ptr<ASTOpConditional> Op = new ASTOpConditional();
				Op->type = OP_TYPE::ME_CONDITIONAL;
				Op->no = lastMeshOp;
				Op->yes = ModifiedMeshOp;
				Op->condition = FullCondition;
				lastMeshOp = Op;
			}
		}
			
		m_activeTags.Pop();

		return lastMeshOp;
	}


	Ptr<ASTOp> CodeGenerator::GenerateDefaultTableValue(ETableColumnType NodeType)
	{
		switch (NodeType)
		{
		case mu::ETableColumnType::Scalar:
		{
			//TODO(Max):MTBL-1660
			//Ptr<NodeScalarConstant> pNode = new NodeScalarConstant();
			//pNode->SetValue(-UE_MAX_FLT);
			//
			//return Generate(pNode);
			return nullptr;
		}
		case mu::ETableColumnType::Color:
		{
			mu::Ptr<mu::NodeColourConstant> pNode = new NodeColourConstant();
			pNode->SetValue(mu::DefaultMutableColorValue);

			FColorGenerationResult ChildResult;
			FGenericGenerationOptions Options;
			GenerateColor(ChildResult, Options, pNode);
			return ChildResult.op;
		}
		case mu::ETableColumnType::Image:
		{
			//TODO(Max):MTBL-1660
			//FImageGenerationOptions DummyOptions;
			//FImageGenerationResult DefaultValue;
			//
			//mu::Ptr<mu::NodeImageReference> ImageNode = new mu::NodeImageReference();
			//ImageNode->SetImageReference(-1);
			//
			//GenerateImage(DummyOptions, DefaultValue, ImageNode);
			//
			//return DefaultValue.op;
			return nullptr;
		}
		case mu::ETableColumnType::Mesh:
			/* The default mesh is always null */
			return nullptr;
		default:
			break;
		}

		return nullptr;
	}

}
