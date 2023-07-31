// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpMeshApplyShape.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshClipDeform.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshExtractLayoutBlocks.h"
#include "MuT/ASTOpMeshFormat.h"
#include "MuT/ASTOpMeshGeometryOperation.h"
#include "MuT/ASTOpMeshMorphReshape.h"
#include "MuT/ASTOpMeshTransform.h"
#include "MuT/ASTOpMeshDifference.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeLayoutPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshApplyPosePrivate.h"
#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeMeshClipDeformPrivate.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/NodeMeshClipMorphPlanePrivate.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/NodeMeshClipWithMeshPrivate.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshConstantPrivate.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFormatPrivate.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshGeometryOperationPrivate.h"
#include "MuT/NodeMeshInterpolate.h"
#include "MuT/NodeMeshInterpolatePrivate.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshMakeMorphPrivate.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshMorphPrivate.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeMeshReshapePrivate.h"
#include "MuT/NodeMeshSubtract.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeMeshSwitchPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeMeshTransformPrivate.h"
#include "MuT/NodeMeshVariation.h"
#include "MuT/NodeMeshVariationPrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"

#include <memory>
#include <utility>


namespace mu
{
class Node;

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::PrepareForLayout(Ptr<const Layout> GeneratedLayout,
		MeshPtr currentLayoutMesh,
		size_t currentLayoutChannel,
		const void* errorContext )
    {
        if (currentLayoutMesh->GetVertexCount()==0)
        {
            return;
        }

		// The layout we are adding must have block ids.
		check(GeneratedLayout->m_blocks.IsEmpty() || GeneratedLayout->m_blocks[0].m_id != -1);

        // 
		Ptr<const Layout> pLayout = GeneratedLayout;
        currentLayoutMesh->AddLayout( pLayout );

        int buffer = -1;
        int channel = -1;
        currentLayoutMesh->GetVertexBuffers().FindChannel( MBS_TEXCOORDS,
                                                              (int)currentLayoutChannel,
                                                              &buffer,
                                                              &channel );
        check( buffer>=0 );
        check( channel>=0 );

        // Create the layout block vertex buffer
        uint16* pLayoutData = 0;
        {
            int layoutBuf = currentLayoutMesh->GetVertexBuffers().GetBufferCount();
            currentLayoutMesh->GetVertexBuffers().SetBufferCount( layoutBuf+1 );

            // TODO
            check( pLayout->GetBlockCount()<65535 );
            MESH_BUFFER_SEMANTIC semantic = MBS_LAYOUTBLOCK;
            int semanticIndex = int(currentLayoutChannel);
            MESH_BUFFER_FORMAT format = MBF_UINT16;
            int components = 1;
            int offset = 0;
            currentLayoutMesh->GetVertexBuffers().SetBuffer
                    (
                        layoutBuf,
                        sizeof(uint16),
                        1,
                        &semantic, &semanticIndex,
                        &format, &components,
                        &offset
                    );
            pLayoutData = (uint16*)currentLayoutMesh->GetVertexBuffers().GetBufferData( layoutBuf );
        }

        // Get the information about the texture coordinates channel
        MESH_BUFFER_SEMANTIC semantic;
        int semanticIndex;
        MESH_BUFFER_FORMAT format;
        int components;
        int offset;
        currentLayoutMesh->GetVertexBuffers().GetChannel
            ( buffer, channel, &semantic, &semanticIndex, &format, &components, &offset );
        check( semantic == MBS_TEXCOORDS );

        const uint8_t* pData = currentLayoutMesh->GetVertexBuffers().GetBufferData( buffer );
        int elemSize = currentLayoutMesh->GetVertexBuffers().GetElementSize( buffer );
        int channelOffset = currentLayoutMesh->GetVertexBuffers().GetChannelOffset( buffer, channel );
        pData += channelOffset;

        // Clear block data
        for (int i=0; i< currentLayoutMesh->GetVertexBuffers().GetElementCount(); ++i)
        {
            pLayoutData[i] = 65535;
        }

        // TODO: Check overlapping layout blocks
        // TODO: Check triangles crossing blocks
        int inside = 0;
        int temp = 0;
        for ( int b=0; b<pLayout->GetBlockCount(); ++b )
        {
            FIntPoint grid = pLayout->GetGridSize();

            box< vec2<int> > block;
            pLayout->GetBlock
                ( b, &block.min[0], &block.min[1], &block.size[0], &block.size[1] );

            box< vec2<float> > rect;
            rect.min[0] = ( (float)block.min[0] ) / (float) grid[0];
            rect.min[1] = ( (float)block.min[1] ) / (float) grid[1];
            rect.size[0] = ( (float)block.size[0] ) / (float) grid[0];
            rect.size[1] = ( (float)block.size[1] ) / (float) grid[1];

            const uint8_t* pVertices = pData;

            if ( format == MBF_FLOAT32 )
            {
                for ( int v=0; v<currentLayoutMesh->GetVertexBuffers().GetElementCount(); ++v )
                {
                    vec2<float>* pUV = (vec2<float>*)pVertices;
                    pVertices += elemSize;

                    if ( pLayoutData[v]==65535 && rect.ContainsInclusive(*pUV) )
                    {
                        *pUV = rect.Homogenize( *pUV );

                        // Set the value to the unique block id
                        check( pLayout->m_blocks[b].m_id < 65535 );
                        pLayoutData[v] =(uint16) pLayout->m_blocks[b].m_id;

                        inside++;
                    }
                    else
                    {
                        temp++;
                    }
                }
            }
            else if ( format == MBF_FLOAT16 )
            {
                // TODO: Very slow?
                for ( int v=0; v<currentLayoutMesh->GetVertexBuffers().GetElementCount(); ++v )
                {
                    float16* pUV = (float16*)pVertices;
                    pVertices += elemSize;

                    vec2<float> UV( halfToFloat(pUV[0]), halfToFloat(pUV[1]) );

                    if ( pLayoutData[v]==65535 && rect.ContainsInclusive(UV) )
                    {
                        UV = rect.Homogenize( UV );

                        // Set the value to the unique block id
                        check( pLayout->m_blocks[b].m_id < 65535 );
                        pLayoutData[v] = (uint16)pLayout->m_blocks[b].m_id;

                        inside++;

                        pUV[0] = floatToHalf( UV[0] );
                        pUV[1] = floatToHalf( UV[1] );
                    }
                }
            }
        }

        int outside = currentLayoutMesh->GetVertexBuffers().GetElementCount() - inside;
        if (outside>0)
        {
            char buf[256];
            mutable_snprintf
                (
                    buf, 256,
                    "Source mesh has %d vertices not assigned to any layout block in LOD %d",
					outside, m_currentParents.Last().m_lod
                );
			int blockCount = pLayout->GetBlockCount();
        
            //m_pErrorLog->GetPrivate()->Add( buf, blockCount==1?ELMT_INFO:ELMT_WARNING, errorContext );
            TArray< float > unassignedUVs;
            unassignedUVs.Reserve(64);           
            
            const uint8_t* pVertices = pData;
            for (int i=0; i<currentLayoutMesh->GetVertexBuffers().GetElementCount(); ++i)
            {
                vec2<float> UV;
                if ( format == MBF_FLOAT32 )
                {
                    UV = *((vec2<float>*)pVertices);
                }
                else if ( format == MBF_FLOAT16 )
                {
                    float16* pUV = (float16*)pVertices;
                    UV = vec2<float>( halfToFloat(pUV[0]), halfToFloat(pUV[1]) );
                }

                pVertices += elemSize;

                if (pLayoutData[i]==65535)
                {
                    unassignedUVs.Add(UV[0]);
                    unassignedUVs.Add(UV[1]);
                }
            }

            ErrorLogMessageAttachedDataView attachedDataView;
            attachedDataView.m_unassignedUVs = unassignedUVs.GetData();
            attachedDataView.m_unassignedUVsSize = (size_t)unassignedUVs.Num();

            m_pErrorLog->GetPrivate()->Add( buf, attachedDataView, blockCount==1?ELMT_INFO:ELMT_WARNING, errorContext );
        }
        
        // Assign broken vertices to the first block
        for (int i=0; i< currentLayoutMesh->GetVertexBuffers().GetElementCount(); ++i)
        {
            if ( pLayoutData[i]==65535 )
            {
                pLayoutData[i] = 0;
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh( const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshPtrConst& InUntypedNode)
    {
        if (!InUntypedNode)
        {
            OutResult = FMeshGenerationResult();
            return;
        }

		// Clear bottom-up state
		m_currentBottomUpState.m_address = nullptr;

        // See if it was already generated
		FGeneratedMeshCacheKey Key;
		Key.Node = InUntypedNode;
		Key.Options = InOptions;
        GeneratedMeshMap::ValueType* it = m_generatedMeshes.Find(Key);
        if ( it )
        {
			OutResult = *it;
            return;
        }

		const NodeMesh* Node = InUntypedNode.get();

        // Generate for each different type of node
		switch (Node->GetMeshNodeType())
		{
		case NodeMesh::EType::Constant: GenerateMesh_Constant(InOptions, OutResult, static_cast<const NodeMeshConstant*>(Node)); break;
		case NodeMesh::EType::Format: GenerateMesh_Format(InOptions, OutResult, static_cast<const NodeMeshFormat*>(Node)); break;
		case NodeMesh::EType::Morph: GenerateMesh_Morph(InOptions, OutResult, static_cast<const NodeMeshMorph*>(Node)); break;
		case NodeMesh::EType::MakeMorph: GenerateMesh_MakeMorph(InOptions, OutResult, static_cast<const NodeMeshMakeMorph*>(Node)); break;
		case NodeMesh::EType::Fragment: GenerateMesh_Fragment(InOptions, OutResult, static_cast<const NodeMeshFragment*>(Node)); break;
		case NodeMesh::EType::Interpolate: GenerateMesh_Interpolate(InOptions, OutResult, static_cast<const NodeMeshInterpolate*>(Node)); break;
		case NodeMesh::EType::Switch: GenerateMesh_Switch(InOptions, OutResult, static_cast<const NodeMeshSwitch*>(Node)); break;
		case NodeMesh::EType::Subtract: GenerateMesh_Subtract(InOptions, OutResult, static_cast<const NodeMeshSubtract*>(Node)); break;
		case NodeMesh::EType::Transform: GenerateMesh_Transform(InOptions, OutResult, static_cast<const NodeMeshTransform*>(Node)); break;
		case NodeMesh::EType::ClipMorphPlane: GenerateMesh_ClipMorphPlane(InOptions, OutResult, static_cast<const NodeMeshClipMorphPlane*>(Node)); break;
		case NodeMesh::EType::ClipWithMesh: GenerateMesh_ClipWithMesh(InOptions, OutResult, static_cast<const NodeMeshClipWithMesh*>(Node)); break;
		case NodeMesh::EType::ApplyPose: GenerateMesh_ApplyPose(InOptions, OutResult, static_cast<const NodeMeshApplyPose*>(Node)); break;
		case NodeMesh::EType::Variation: GenerateMesh_Variation(InOptions, OutResult, static_cast<const NodeMeshVariation*>(Node)); break;
		case NodeMesh::EType::Table: GenerateMesh_Table(InOptions, OutResult, static_cast<const NodeMeshTable*>(Node)); break;
		case NodeMesh::EType::GeometryOperation: GenerateMesh_GeometryOperation(InOptions, OutResult, static_cast<const NodeMeshGeometryOperation*>(Node)); break;
		case NodeMesh::EType::Reshape: GenerateMesh_Reshape(InOptions, OutResult, static_cast<const NodeMeshReshape*>(Node)); break;
		case NodeMesh::EType::ClipDeform: GenerateMesh_ClipDeform(InOptions, OutResult, static_cast<const NodeMeshClipDeform*>(Node)); break;
		case NodeMesh::EType::None: check(false);
		}

        // Cache the result
        m_generatedMeshes.Add( Key, OutResult);
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Morph(
		const FMeshGenerationOptions& InOptions, 
		FMeshGenerationResult& OutResult, 
		const NodeMeshMorph* InMorphNode 
	)
    {
        NodeMeshMorph::Private& node = *InMorphNode->GetPrivate();

        Ptr<ASTOpMeshMorph> OpMorph = new ASTOpMeshMorph();

        // Factor
        if ( node.m_pFactor )
        {
            OpMorph->Factor = Generate( node.m_pFactor.get() );
        }
        else
        {
            // This argument is required
            OpMorph->Factor = GenerateMissingScalarCode( "Morph factor", 0.5f, node.m_errorContext );
        }

        // Base
        FMeshGenerationResult BaseResult;
        if ( node.m_pBase )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;
            GenerateMesh(BaseOptions,BaseResult, node.m_pBase );
            OpMorph->Base = BaseResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh morph base node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }        

        for ( int32 t=0 ; t<node.m_morphs.Num(); ++t )
        {
            FMeshGenerationResult TargetResult;
			FMeshGenerationOptions TargetOptions = InOptions;
			TargetOptions.bUniqueVertexIDs = false;
			TargetOptions.bLayouts = false;
			TargetOptions.OverrideLayouts.Empty();
			TargetOptions.ActiveTags.Empty();
            GenerateMesh(TargetOptions, TargetResult, node.m_morphs[t]);

            // TODO: Make sure that the target is a mesh with the morph format
            Ptr<ASTOp> target = TargetResult.meshOp;

            // If the vertex indices are supposed to be relative in the targets, adjust them
            //if (node.m_vertexIndicesAreRelative)
            //{
            //    Ptr<ASTOpMeshRemapIndices> remapIndices = new ASTOpMeshRemapIndices;
            //    remapIndices->source = target;
            //    remapIndices->reference = baseResult.baseMeshOp;
            //    target = remapIndices;
            //}

            OpMorph->AddTarget( target );
        }
 
        const bool bReshapeEnabled = node.m_reshapeSkeleton || node.m_reshapePhysicsVolumes;
        
        Ptr<ASTOpMeshMorphReshape> OpMorphReshape;
        if ( bReshapeEnabled )
        {
		    Ptr<ASTOpMeshBindShape> OpBind = new ASTOpMeshBindShape();
		    Ptr<ASTOpMeshApplyShape> OpApply = new ASTOpMeshApplyShape();

			// Setting reshapeVertices to false the bind op will remove all mesh members except 
			// PhysicsBodies and the Skeleton.
            OpBind->m_reshapeVertices = false;
		    OpBind->m_reshapeSkeleton = node.m_reshapeSkeleton;
		    OpBind->m_deformAllBones = false; //TODO: Remove deprecated var from node
		    OpBind->m_bonesToDeform = node.m_bonesToDeform;
    	    OpBind->m_reshapePhysicsVolumes = node.m_reshapePhysicsVolumes; 
			OpBind->m_physicsToDeform = node.m_physicsToDeform;
			OpBind->m_deformAllPhysics = node.m_deformAllPhysics;
			OpBind->m_bindingMethod = static_cast<uint32>(EShapeBindingMethod::ReshapeClosestProject);
            
			OpBind->Mesh = BaseResult.meshOp;
            OpBind->Shape = BaseResult.meshOp;
           
			OpApply->m_reshapeVertices = OpBind->m_reshapeVertices;
		    OpApply->m_reshapeSkeleton = OpBind->m_reshapeSkeleton;
		    OpApply->m_reshapePhysicsVolumes = OpBind->m_reshapePhysicsVolumes;

			OpApply->Mesh = OpBind;
            OpApply->Shape = OpMorph;

            OpMorphReshape = new ASTOpMeshMorphReshape();
            OpMorphReshape->Morph = OpMorph;
            OpMorphReshape->Reshape = OpApply;
        }

 		if (OpMorphReshape)
		{
			OutResult.meshOp = OpMorphReshape;
		}
		else
		{
			OutResult.meshOp = OpMorph;
		}

        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_MakeMorph(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshMakeMorph* InMakeMorphNode )
    {
        NodeMeshMakeMorph::Private& node = *InMakeMorphNode->GetPrivate();

        Ptr<ASTOpMeshDifference> op = new ASTOpMeshDifference();

        // \todo Texcoords are broken?
        op->bIgnoreTextureCoords = true;

        // Base
        FMeshGenerationResult BaseResult;
        if ( node.m_pBase )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;
			BaseOptions.bLayouts = false;
			GenerateMesh(BaseOptions, BaseResult, node.m_pBase );

            op->Base = BaseResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh make morph base node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        // Target
		if ( node.m_pTarget )
        {
			FMeshGenerationOptions TargetOptions = InOptions;
			TargetOptions.bUniqueVertexIDs = false;
			TargetOptions.bLayouts = false;
			TargetOptions.OverrideLayouts.Empty();
			TargetOptions.ActiveTags.Empty();
			FMeshGenerationResult TargetResult;
            GenerateMesh( TargetOptions, TargetResult, node.m_pTarget );

            op->Target = TargetResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh make morph target node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        OutResult.meshOp = op;
        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
	}

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Fragment(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshFragment* fragment )
    {
        NodeMeshFragment::Private& node = *fragment->GetPrivate();

        FMeshGenerationResult BaseResult;
        if ( node.m_pMesh )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			if (node.m_fragmentType == NodeMeshFragment::FT_LAYOUT_BLOCKS)
			{
				BaseOptions.bLayouts = true;
			}
            GenerateMesh( BaseOptions, BaseResult, node.m_pMesh );

            if ( node.m_fragmentType==NodeMeshFragment::FT_LAYOUT_BLOCKS )
            {
                Ptr<ASTOpMeshExtractLayoutBlocks> op = new ASTOpMeshExtractLayoutBlocks();
                OutResult.meshOp = op;

                op->source = BaseResult.meshOp;

                if (BaseResult.GeneratedLayouts.Num()>node.m_layoutOrGroup )
                {
                    const Layout* pLayout = BaseResult.GeneratedLayouts[node.m_layoutOrGroup].get();
                    op->layout = (uint16)node.m_layoutOrGroup;

                    for ( int32 i=0; i<node.m_blocks.Num(); ++i )
                    {
                        if (node.m_blocks[i]>=0 && node.m_blocks[i]<pLayout->m_blocks.Num() )
                        {
                            int bid = pLayout->m_blocks[ node.m_blocks[i] ].m_id;
                            op->blocks.Add(bid);
                        }
                        else
                        {
                            m_pErrorLog->GetPrivate()->Add( "Internal layout block index error.",
                                                            ELMT_ERROR, node.m_errorContext );
                        }
                    }
                }
                else
                {
                    // This argument is required
                    m_pErrorLog->GetPrivate()->Add( "Missing layout in mesh fragment source.",
                                                    ELMT_ERROR, node.m_errorContext );
                }
            }

            else if ( node.m_fragmentType==NodeMeshFragment::FT_FACE_GROUP )
            {
				// \TODO: Deprecated?
                Ptr<ASTOpFixed> op = new ASTOpFixed();
                OutResult.meshOp = op;

                op->op.type = OP_TYPE::ME_EXTRACTFACEGROUP;

                op->SetChild( op->op.args.MeshExtractFaceGroup.source, BaseResult.meshOp );
                op->op.args.MeshExtractFaceGroup.group = node.m_layoutOrGroup;
            }

        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh fragment source is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Interpolate(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshInterpolate* interpolate )
    {
        NodeMeshInterpolate::Private& node = *interpolate->GetPrivate();

        // Generate the code
        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::ME_INTERPOLATE;
        OutResult.meshOp = op;

        // Factor
        if ( Node* pFactor = node.m_pFactor.get() )
        {
            op->SetChild( op->op.args.MeshInterpolate.factor, Generate( pFactor ) );
        }
        else
        {
            // This argument is required
            op->SetChild( op->op.args.MeshInterpolate.factor,
                          GenerateMissingScalarCode( "Interpolation factor", 0.5f, node.m_errorContext ) );
        }

        //
        Ptr<ASTOp> base = 0;
        int count = 0;
        for ( int32 t=0
            ; t<node.m_targets.Num() && t<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1
            ; ++t )
        {
            if ( NodeMesh* pA = node.m_targets[t].get() )
            {
				FMeshGenerationOptions TargetOptions = InOptions;
				TargetOptions.bUniqueVertexIDs = true;
				TargetOptions.OverrideLayouts.Empty();

                FMeshGenerationResult TargetResult;
                GenerateMesh( TargetOptions, TargetResult, pA );


                // The first target is the base
                if (count==0)
                {
                    base = TargetResult.meshOp;
                    op->SetChild( op->op.args.MeshInterpolate.base, TargetResult.meshOp );

                    OutResult.baseMeshOp = TargetResult.baseMeshOp;
					OutResult.GeneratedLayouts = TargetResult.GeneratedLayouts;
				}
                else
                {
                    Ptr<ASTOpMeshDifference> dop = new ASTOpMeshDifference();
                    dop->Base = base;
                    dop->Target = TargetResult.meshOp;

                    // \todo Texcoords are broken?
                    dop->bIgnoreTextureCoords = true;

                    for ( size_t c=0; c<node.m_channels.Num(); ++c)
                    {
                        check( node.m_channels[c].semantic < 256 );
						check(node.m_channels[c].semanticIndex < 256);
						
						ASTOpMeshDifference::FChannel Channel;
						Channel.Semantic = uint8(node.m_channels[c].semantic);
						Channel.SemanticIndex = uint8(node.m_channels[c].semanticIndex);
						dop->Channels.Add(Channel);
                    }

                    op->SetChild( op->op.args.MeshInterpolate.targets[count-1], dop );
                }
                count++;
            }
        }

        // At least one mesh is required
        if (!count)
        {
            // TODO
            //op.args.MeshInterpolate.target[0] = GenerateMissingImageCode( "First mesh", IF_RGB_UBYTE );
            m_pErrorLog->GetPrivate()->Add
                ( "Mesh interpolation: at least the first mesh is required.",
                  ELMT_ERROR, node.m_errorContext );
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Switch(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshSwitch* sw )
    {
        NodeMeshSwitch::Private& node = *sw->GetPrivate();

        if (node.m_options.Num() == 0)
        {
            // No options in the switch!
            // TODO
            OutResult = FMeshGenerationResult();
			return;
        }

        Ptr<ASTOpSwitch> op = new ASTOpSwitch();
        op->type = OP_TYPE::ME_SWITCH;

        // Factor
        if ( node.m_pParameter )
        {
            op->variable = Generate( node.m_pParameter.get() );
        }
        else
        {
            // This argument is required
            op->variable = GenerateMissingScalarCode( "Switch variable", 0.0f, node.m_errorContext );
        }

        // Options
        for ( int32 t=0; t< node.m_options.Num(); ++t )
        {
			FMeshGenerationOptions TargetOptions = InOptions;

            if (t!=0)
            {
				TargetOptions.OverrideLayouts = OutResult.GeneratedLayouts;
            }

            if ( node.m_options[t] )
            {
                FMeshGenerationResult BranchResults;
                GenerateMesh(TargetOptions, BranchResults, node.m_options[t] );

                auto branch = BranchResults.meshOp;
                op->cases.Emplace((int16)t,op,branch);

                if (t==0)
                {
                    OutResult = BranchResults;
                }
            }
        }

        OutResult.meshOp = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_Table(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshTable* TableNode)
	{
		//
		FMeshGenerationResult NewResult = OutResult;
		int t = 0;

		Ptr<ASTOp> Op = GenerateTableSwitch<NodeMeshTable::Private, TCT_MESH, OP_TYPE::ME_SWITCH>(*TableNode->GetPrivate(), 
			[this, &NewResult, &t, &InOptions] (const NodeMeshTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				NodeMeshConstantPtr pCell = new NodeMeshConstant();
				MeshPtr pMesh = node.m_pTable->GetPrivate()->m_rows[row].m_values[colIndex].m_pMesh;

				if (!pMesh)
				{
					char temp[256];
					mutable_snprintf(temp, 256,
						"Table has a missing mesh in column %d, row %d.", colIndex, row);
					pErrorLog->GetPrivate()->Add(temp, ELMT_ERROR, node.m_errorContext);
				}

				pCell->SetValue(pMesh);

				// TODO Take into account layout strategy
				int numLayouts = node.m_layouts.Num();
				pCell->SetLayoutCount(numLayouts);
				for (int i = 0; i < numLayouts; ++i)
				{
					pCell->SetLayout(i, node.m_layouts[i]);
				}

				FMeshGenerationOptions TargetOptions = InOptions;

				if (t != 0)
				{
					TargetOptions.OverrideLayouts = NewResult.GeneratedLayouts;
				}

				FMeshGenerationResult BranchResults;
				GenerateMesh(TargetOptions, BranchResults, pCell);

				if (t == 0)
				{
					NewResult = BranchResults;
				}				

				++t;
				return BranchResults.meshOp;
			});

		NewResult.meshOp = Op;

		OutResult = NewResult;
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Variation(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshVariation* va )
    {
        NodeMeshVariation::Private& node = *va->GetPrivate();

        FMeshGenerationResult currentResult;
        Ptr<ASTOp> currentMeshOp;

        bool firstOptionProcessed = false;

        // Default case
        if ( node.m_defaultMesh )
        {
            FMeshGenerationResult BranchResults;
			FMeshGenerationOptions DefaultOptions = InOptions;

			GenerateMesh(DefaultOptions, BranchResults, node.m_defaultMesh );
            currentMeshOp = BranchResults.meshOp;
            currentResult = BranchResults;
            firstOptionProcessed = true;
        }

        // Process variations in reverse order, since conditionals are built bottom-up.
        for ( int32 t = node.m_variations.Num()-1; t >= 0; --t )
        {
            int tagIndex = -1;
            const string& tag = node.m_variations[t].m_tag;
            for ( int i = 0; i < m_firstPass.m_tags.Num(); ++i )
            {
                if ( m_firstPass.m_tags[i].tag==tag)
                {
                    tagIndex = i;
                }
            }

            if ( tagIndex < 0 )
            {
                m_pErrorLog->GetPrivate()->Add( 
					FString::Printf(TEXT("Unknown tag found in mesh variation [%s]."), tag.c_str()), 
					ELMT_WARNING, 
					node.m_errorContext
				);
                continue;
            }

            Ptr<ASTOp> variationMeshOp;
            if ( node.m_variations[t].m_mesh )
            {
				FMeshGenerationOptions VariationOptions = InOptions;

                if (firstOptionProcessed)
                {
					VariationOptions.OverrideLayouts = currentResult.GeneratedLayouts;
                }
         
                FMeshGenerationResult BranchResults;
				GenerateMesh(VariationOptions, BranchResults, node.m_variations[t].m_mesh );

                variationMeshOp = BranchResults.meshOp;

                if ( !firstOptionProcessed )
                {
                    firstOptionProcessed = true;                   
                    currentResult = BranchResults;
                }
            }

            Ptr<ASTOpConditional> conditional = new ASTOpConditional;
            conditional->type = OP_TYPE::ME_CONDITIONAL;
            conditional->no = currentMeshOp;
            conditional->yes = variationMeshOp;            
            conditional->condition = m_firstPass.m_tags[tagIndex].genericCondition;

            currentMeshOp = conditional;
        }

        OutResult = currentResult;
        OutResult.meshOp = currentMeshOp;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Constant(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshConstant* constant )
    {
        NodeMeshConstant::Private& node = *constant->GetPrivate();

        Ptr<ASTOpConstantResource> op = new ASTOpConstantResource();
        op->type = OP_TYPE::ME_CONSTANT;
		OutResult.baseMeshOp = op;
		OutResult.meshOp = op;
		OutResult.GeneratedLayouts.Empty();

		bool bIsOverridingLayouts = !InOptions.OverrideLayouts.IsEmpty();

        MeshPtr pMesh = node.m_pValue.get();
		if (!pMesh)
		{
			// This data is required
			MeshPtr pTempMesh = new Mesh();
			op->SetValue(pTempMesh, m_compilerOptions->m_optimisationOptions.m_useDiskCache);
			m_constantMeshes.Add(pTempMesh);

			// Log an error message
			m_pErrorLog->GetPrivate()->Add("Constant mesh not set.", ELMT_WARNING, node.m_errorContext);

			return;
		}

		// Find out if we can (or have to) reuse a mesh that we have already generated.
		MeshPtrConst DuplicateOf;
		for (int32 i = 0; i < m_constantMeshes.Num(); ++i)
		{
			MeshPtrConst Candidate = m_constantMeshes[i];
			
			bool bCompareLayouts = InOptions.bLayouts && !bIsOverridingLayouts;

			if (Candidate->IsSimilar(*pMesh, bCompareLayouts))
			{
				// If it is similar, and we need unique vertex IDs, check that it also has them. This was skipped in the IsSimilar.
				if (InOptions.bUniqueVertexIDs)
				{
					int32 FoundBuffer = -1;
					int32 FoundChannel = -1;
					Candidate->GetVertexBuffers().FindChannel(MBS_VERTEXINDEX, 0, &FoundBuffer, &FoundChannel);
					bool bHasUniqueVertexIDs = FoundBuffer >= 0 && FoundChannel >= 0;
					if (!bHasUniqueVertexIDs)
					{
						continue;
					}
				}

				// If it is similar and we are overriding the layouts, we must compare the layouts of the candidate with the ones
				// we are using to override.
				if (InOptions.bLayouts && bIsOverridingLayouts)
				{
					if (Candidate->GetLayoutCount() != InOptions.OverrideLayouts.Num())
					{
						continue;
					}

					bool bLayoutsAreEqual = true;
					for (int32 l = 0; l < Candidate->GetLayoutCount(); ++l)
					{
						bLayoutsAreEqual = (*Candidate->GetLayout(l) == *InOptions.OverrideLayouts[l]);
						if ( !bLayoutsAreEqual )
						{
							break;
						}
					}

					if (!bLayoutsAreEqual)
					{
						continue;
					}
				}

				DuplicateOf = Candidate;
				break;
			}
		}

		if (DuplicateOf)
		{
			// Make sure the source layouts of the mesh are mapped to the layouts of the duplicated mesh.
			if (InOptions.bLayouts)
			{
				if (bIsOverridingLayouts)
				{
					for (int32 l = 0; l < DuplicateOf->GetLayoutCount(); ++l)
					{
						const Layout* OverridingLayout = InOptions.OverrideLayouts[l].get();
						OutResult.GeneratedLayouts.Add(OverridingLayout);
					}
				}
				else
				{
					for (int32 l = 0; l < DuplicateOf->GetLayoutCount(); ++l)
					{
						const Layout* DuplicatedLayout = DuplicateOf->GetLayout(l);
						OutResult.GeneratedLayouts.Add(DuplicatedLayout);
					}
				}
			}

			op->SetValue(DuplicateOf, m_compilerOptions->m_optimisationOptions.m_useDiskCache);
		}
		else
		{
			// We need to clone the mesh in the node because we will modify it.
			Ptr<Mesh> pCloned = pMesh->Clone();
			pCloned->EnsureSurfaceData();

			if (InOptions.bLayouts)
			{
				if (!bIsOverridingLayouts)
				{
					// Apply whatever transform is necessary for every layout
					for (int32 LayoutIndex = 0; LayoutIndex < node.m_layouts.Num(); ++LayoutIndex)
					{
						NodeLayoutPtr pLayoutNode = node.m_layouts[LayoutIndex];
						// TODO: In a cleanup of the design of the layouts, we should remove this cast.
						const NodeLayoutBlocks* TypedNode = dynamic_cast<NodeLayoutBlocks*>(pLayoutNode.get());
						if (TypedNode)
						{
							Ptr<const Layout> SourceLayout = TypedNode->GetPrivate()->m_pLayout;
							Ptr<const Layout> GeneratedLayout = AddLayout( SourceLayout );
							PrepareForLayout(GeneratedLayout, pCloned, LayoutIndex, TypedNode->GetPrivate()->m_errorContext);

							OutResult.GeneratedLayouts.Add(GeneratedLayout);
						}
					}
				}
				else
				{
					// We need to apply the transform of the layouts used to override
					for (int32 LayoutIndex = 0; LayoutIndex < InOptions.OverrideLayouts.Num(); ++LayoutIndex)
					{
						Ptr<const Layout> GeneratedLayout = InOptions.OverrideLayouts[LayoutIndex];
						PrepareForLayout(GeneratedLayout, pCloned, LayoutIndex, node.m_errorContext);

						OutResult.GeneratedLayouts.Add(GeneratedLayout);
					}
				}
			}

			if (InOptions.bUniqueVertexIDs)
			{
				// Enumerate the vertices uniquely unless they already have indices
				int buf = -1;
				int chan = -1;
				pCloned->GetVertexBuffers().FindChannel(MBS_VERTEXINDEX, 0, &buf, &chan);
				bool hasVertexIndices = (buf >= 0 && chan >= 0);
				if (!hasVertexIndices)
				{
					int newBuffer = pCloned->GetVertexBuffers().GetBufferCount();
					pCloned->GetVertexBuffers().SetBufferCount(newBuffer + 1);
					MESH_BUFFER_SEMANTIC semantic = MBS_VERTEXINDEX;
					int semanticIndex = 0;
					MESH_BUFFER_FORMAT format = MBF_UINT32;
					int components = 1;
					int offset = 0;
					pCloned->GetVertexBuffers().SetBuffer
					(
						newBuffer,
						sizeof(uint32),
						1,
						&semantic, &semanticIndex,
						&format, &components,
						&offset
					);
					uint32* pIdData = (uint32*)pCloned->GetVertexBuffers().GetBufferData(newBuffer);
					for (int i = 0; i < pMesh->GetVertexCount(); ++i)
					{
						check(m_freeVertexIndex < std::numeric_limits<uint32>::max());

						(*pIdData++) = m_freeVertexIndex++;
						check(m_freeVertexIndex < std::numeric_limits<uint32>::max());
					}
				}
			}

			// Add the constant data
			m_constantMeshes.Add(pCloned);
			op->SetValue(pCloned.get(), m_compilerOptions->m_optimisationOptions.m_useDiskCache);
		}

 
		// Apply the modifier for the pre-normal operations stage.
		BOTTOM_UP_STATE temp = m_currentBottomUpState;

		bool bModifiersForBeforeOperations = true;
		OutResult.meshOp = ApplyMeshModifiers(op, InOptions.ActiveTags,
			bModifiersForBeforeOperations, node.m_errorContext);

		m_currentBottomUpState = temp;

    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Format(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshFormat* format )
    {
        NodeMeshFormat::Private& node = *format->GetPrivate();

        if ( node.m_pSource )
        {
			FMeshGenerationOptions Options = InOptions;

			FMeshGenerationResult baseResult;
			GenerateMesh(Options,baseResult, node.m_pSource);
            Ptr<ASTOpMeshFormat> op = new ASTOpMeshFormat();
            op->Source = baseResult.meshOp;
            op->Buffers = 0;

            MeshPtr pFormatMesh = new Mesh();

            if (node.m_VertexBuffers.GetBufferCount())
            {
                op->Buffers |= OP::MeshFormatArgs::BT_VERTEX;
                pFormatMesh->m_VertexBuffers = node.m_VertexBuffers;
            }

            if (node.m_IndexBuffers.GetBufferCount())
            {
				op->Buffers |= OP::MeshFormatArgs::BT_INDEX;
                pFormatMesh->m_IndexBuffers = node.m_IndexBuffers;
            }

            if (node.m_FaceBuffers.GetBufferCount())
            {
                op->Buffers |= OP::MeshFormatArgs::BT_FACE;
                pFormatMesh->m_FaceBuffers = node.m_FaceBuffers;
            }

            Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
            cop->type = OP_TYPE::ME_CONSTANT;
            cop->SetValue( pFormatMesh, m_compilerOptions->m_optimisationOptions.m_useDiskCache );
            op->Format = cop;

            m_constantMeshes.Add(pFormatMesh);

            OutResult.meshOp = op;
            OutResult.baseMeshOp = baseResult.baseMeshOp;
			OutResult.GeneratedLayouts = baseResult.GeneratedLayouts;
		}
        else
        {
            // Put something there
            GenerateMesh(InOptions, OutResult, new NodeMeshConstant() );
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Subtract(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshSubtract* subs )
    {
		// This node is deprecated.
		check(false);        
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Transform(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                const NodeMeshTransform* trans )
    {
        const auto& node = *trans->GetPrivate();

        Ptr<ASTOpMeshTransform> op = new ASTOpMeshTransform();

        // Base
        if (node.m_pSource)
        {
            GenerateMesh(InOptions, OutResult, node.m_pSource);
            op->source = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh transform base node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        op->matrix = node.m_transform;

        OutResult.meshOp = op;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ClipMorphPlane(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                     const NodeMeshClipMorphPlane* clip )
    {
        const auto& node = *clip->GetPrivate();

        Ptr<ASTOpMeshClipMorphPlane> op = new ASTOpMeshClipMorphPlane();

        // Base
        if (node.m_pSource)
        {
            GenerateMesh(InOptions, OutResult, node.m_pSource);
            op->source = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-morph-plane source node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Morph to an ellipse
        {
            op->morphShape.type = (uint8_t)SHAPE::Type::Ellipse;
            op->morphShape.position = node.m_origin;
            op->morphShape.up = node.m_normal;
            op->morphShape.size = vec3f(node.m_radius1, node.m_radius2, node.m_rotation); // TODO: Move rotation to ellipse rotation reference base instead of passing it directly

                                                                                      // Generate a "side" vector.
                                                                                      // \todo: make generic and move to the vector class
            {
                // Generate vector perpendicular to normal for ellipse rotation reference base
                vec3f aux_base(0.f, 1.f, 0.f);

                if (fabs(dot(node.m_normal, aux_base)) > 0.95f)
                {
                    aux_base = vec3f(0.f, 0.f, 1.f);
                }

                op->morphShape.side = cross(node.m_normal, aux_base);
            }
        }

        // Selection by shape
        if (node.m_vertexSelectionType== NodeMeshClipMorphPlane::Private::VS_SHAPE)
        {
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_SHAPE;
            op->selectionShape.type = (uint8_t)SHAPE::Type::AABox;
            op->selectionShape.position = node.m_selectionBoxOrigin;
            op->selectionShape.size = node.m_selectionBoxRadius;
        }
        else if (node.m_vertexSelectionType == NodeMeshClipMorphPlane::Private::VS_BONE_HIERARCHY)
        {
            // Selection by bone hierarchy?
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY;
            op->vertexSelectionBone = node.m_vertexSelectionBone;
			op->vertexSelectionBoneMaxRadius = node.m_maxEffectRadius;
        }
        else
        {
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_NONE;
        }

        // Parameters
        op->dist = node.m_dist;
        op->factor = node.m_factor;

        OutResult.meshOp = op;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ClipWithMesh(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                   const NodeMeshClipWithMesh* clip )
    {
        const auto& node = *clip->GetPrivate();

        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::ME_CLIPWITHMESH;

        // Base
        if (node.m_pSource)
        {
            GenerateMesh( InOptions, OutResult, node.m_pSource );
            op->SetChild( op->op.args.MeshClipWithMesh.source, OutResult.meshOp );
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-with-mesh source node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Clipping mesh
        if (node.m_pClipMesh)
        {
			FMeshGenerationOptions ClipOptions = InOptions;
			ClipOptions.bUniqueVertexIDs = false;
			ClipOptions.bLayouts = false;
			ClipOptions.OverrideLayouts.Empty();
			ClipOptions.ActiveTags.Empty();

            FMeshGenerationResult clipResult;
            GenerateMesh(ClipOptions, clipResult, node.m_pClipMesh);
            op->SetChild( op->op.args.MeshClipWithMesh.clipMesh, clipResult.meshOp );
		}
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-with-mesh clipping mesh node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        OutResult.meshOp = op;
    }

	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_ClipDeform(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& Result, const NodeMeshClipDeform* ClipDeform)
	{
		const auto& Node = *ClipDeform->GetPrivate();

		const Ptr<ASTOpMeshBindShape> OpBind = new ASTOpMeshBindShape();
		const Ptr<ASTOpMeshClipDeform> OpClipDeform = new ASTOpMeshClipDeform();

		// Base Mesh
		if (Node.m_pBaseMesh)
		{
			GenerateMesh(InOptions, Result, Node.m_pBaseMesh);
			OpBind->Mesh = Result.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh Clip Deform base mesh node is not set.",
				ELMT_ERROR, Node.m_errorContext);
		}

		// Base Shape
		if (Node.m_pClipShape)
		{
			FMeshGenerationOptions ClipOptions = InOptions;
			ClipOptions.bUniqueVertexIDs = false;
			ClipOptions.bLayouts = false;
			ClipOptions.OverrideLayouts.Empty();
			ClipOptions.ActiveTags.Empty();

			FMeshGenerationResult baseResult;
			GenerateMesh(ClipOptions, baseResult, Node.m_pClipShape);
			OpBind->Shape = baseResult.meshOp;
			OpClipDeform->ClipShape = baseResult.meshOp;
		}

    	OpBind->m_discardInvalidBindings = false;
		OpClipDeform->Mesh = OpBind;

		Result.meshOp = OpClipDeform;
	}

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ApplyPose(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                const NodeMeshApplyPose* pose )
    {
        const auto& node = *pose->GetPrivate();

        Ptr<ASTOpMeshApplyPose> op = new ASTOpMeshApplyPose();

        // Base
        if (node.m_pBase)
        {
            GenerateMesh(InOptions, OutResult, node.m_pBase );
            op->base = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh apply-pose base node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Pose mesh
        if (node.m_pPose)
        {
			FMeshGenerationOptions PoseOptions = InOptions;
			PoseOptions.bUniqueVertexIDs = false;
			PoseOptions.bLayouts = false;
			PoseOptions.OverrideLayouts.Empty();
			PoseOptions.ActiveTags.Empty();

            FMeshGenerationResult poseResult;
            GenerateMesh(PoseOptions, poseResult, node.m_pPose );
            op->pose = poseResult.meshOp;
		}
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh apply-pose pose node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        OutResult.meshOp = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_GeometryOperation(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshGeometryOperation* geom)
	{
		const auto& node = *geom->GetPrivate();

		Ptr<ASTOpMeshGeometryOperation> op = new ASTOpMeshGeometryOperation();

		// Mesh A
		if (node.m_pMeshA)
		{
			GenerateMesh(InOptions, OutResult, node.m_pMeshA);
			op->meshA = OutResult.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh geometric op mesh-a node is not set.",
				ELMT_ERROR, node.m_errorContext);
		}

		// Mesh B
		if (node.m_pMeshB)
		{
			FMeshGenerationOptions OtherOptions = InOptions;
			OtherOptions.bUniqueVertexIDs = false;
			OtherOptions.bLayouts = false;
			OtherOptions.OverrideLayouts.Empty();
			OtherOptions.ActiveTags.Empty();

			FMeshGenerationResult bResult;
			GenerateMesh(OtherOptions, bResult, node.m_pMeshB);
			op->meshB = bResult.meshOp;
		}

		op->scalarA = Generate(node.m_pScalarA);
		op->scalarB = Generate(node.m_pScalarB);

		OutResult.meshOp = op;
	}


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_Reshape(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshReshape* reshape)
	{
		const NodeMeshReshape::Private& node = *reshape->GetPrivate();

		Ptr<ASTOpMeshBindShape> opBind = new ASTOpMeshBindShape();
		Ptr<ASTOpMeshApplyShape> opApply = new ASTOpMeshApplyShape();

		opBind->m_reshapeSkeleton = node.m_reshapeSkeleton;
    	opBind->m_enableRigidParts = node.m_enableRigidParts;
		opBind->m_deformAllBones = false; //TODO: Remove deprecated var from node
		opBind->m_bonesToDeform = node.m_bonesToDeform;
    	opBind->m_reshapePhysicsVolumes = node.m_reshapePhysicsVolumes;
		opBind->m_deformAllPhysics = node.m_deformAllPhysics;
		opBind->m_physicsToDeform = node.m_physicsToDeform;
		opBind->m_reshapeVertices = true;
		opBind->m_bindingMethod = static_cast<uint32>(EShapeBindingMethod::ReshapeClosestProject);

		opApply->m_reshapeVertices = true;	
		opApply->m_reshapeSkeleton = node.m_reshapeSkeleton;
		opApply->m_reshapePhysicsVolumes = node.m_reshapePhysicsVolumes;

		// Base Mesh
		if (node.m_pBaseMesh)
		{
			GenerateMesh(InOptions, OutResult, node.m_pBaseMesh);
			opBind->Mesh = OutResult.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh reshape base node is not set.", ELMT_ERROR, node.m_errorContext);
		}

		// Base and target shapes shouldn't have layouts or modifiers.
		FMeshGenerationOptions ShapeOptions = InOptions;
		ShapeOptions.bUniqueVertexIDs = false;
		ShapeOptions.bLayouts = false;
		ShapeOptions.OverrideLayouts.Empty();
		ShapeOptions.ActiveTags.Empty();

		// Base Shape
		if (node.m_pBaseShape)
		{
			FMeshGenerationResult baseResult;
			GenerateMesh(ShapeOptions, baseResult, node.m_pBaseShape);
			opBind->Shape = baseResult.meshOp;
		}

		opApply->Mesh = opBind;

		// Target Shape
		if (node.m_pTargetShape)
		{
			FMeshGenerationResult targetResult;
			GenerateMesh(ShapeOptions, targetResult, node.m_pTargetShape);
			opApply->Shape = targetResult.meshOp;
		}

		OutResult.meshOp = opApply;
	}

}

