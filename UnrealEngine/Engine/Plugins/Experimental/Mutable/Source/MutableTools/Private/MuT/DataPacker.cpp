// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/DataPacker.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "MuR/CodeVisitor.h"
#include "MuR/Image.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/ASTOpMeshExtractLayoutBlocks.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshDifference.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpLayoutFromMesh.h"
#include "MuT/CompilerPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    SubtreeSearchConstantVisitor::SubtreeSearchConstantVisitor( FProgram& program,
                                                                OP::ADDRESS constant,
                                                                OP_TYPE optype )
        : m_program( program ),
          m_constant( constant ),
          m_opType( optype )
    {
        m_visited.SetNum( program.m_opAddress.Num() );
        if ( m_visited.Num() )
        {
            FMemory::Memset( &m_visited[0], 0, m_visited.Num() );
        }
    }


    //---------------------------------------------------------------------------------------------
    bool SubtreeSearchConstantVisitor::Run( OP::ADDRESS root )
    {
        // too spammy
        // \todo: called too many times!
        // MUTABLE_CPUPROFILER_SCOPE(SubtreeSearchConstantVisitor);

        bool found = false;

        TArray< TPair<bool,int> > pending;
        pending.Reserve(m_program.m_opAddress.Num()/4);

		pending.Add({ false,root });

        while( pending.Num() )
        {
			TPair<bool,int> item = pending.Pop();

            if (item.Key)
            {
                // Item indicating we finished with all the children of a parent.

                // Propagate the "found" state upwards
                m_visited[item.Value]=1;

                //OP cop = m_program.m_code[item.Value];
                ForEachReference( m_program, (OP::ADDRESS)item.Value, [&](OP::ADDRESS ref)
                {
                    if (ref && m_visited[ref]==2)
                    {
                        m_visited[item.Value]=2;
                    }
                });
            }
            else
            {
                if ( !m_visited[item.Value] )
                {
                    OP_TYPE thisOpType = m_program.GetOpType(item.Value);
                    if ( m_opType == thisOpType )
                    {
						OP::ResourceConstantArgs args = m_program.GetOpArgs<OP::ResourceConstantArgs>(item.Value);
                        if ( m_constant == args.value )
                        {
                            found = true;
                            m_visited[item.Value] = 2;
                        }
                    }

                    if (!found)
                    {
						pending.Add({ true,item.Value });

                        //OP cop = m_program.m_code[item.Value];
                        ForEachReference( m_program, item.Value, [&](OP::ADDRESS ref)
                        {
                            if (ref && !m_visited[ref])
                            {
								pending.Add({ false,ref });
                            }
                        });
                    }
                }
                else
                {
                    found = (m_visited[item.Value]==2);
                }
            }
        }

        return found;
    }



    //---------------------------------------------------------------------------------------------
    //! Get all the parameters that affect the constant.
    //! "Affect" means that the constant may be used or not depending on the parameter.
    //---------------------------------------------------------------------------------------------
    class GatherParametersVisitor : public UniqueConstCodeVisitorIterative< TArray<int> >
    {
    public:

        GatherParametersVisitor( FProgram& program, OP::ADDRESS constant, OP_TYPE opType )
            : m_constSearch( program, constant, opType )
        {
            MUTABLE_CPUPROFILER_SCOPE(GatherParametersVisitor);

			TArray<int> currentParams;
            currentParams.SetNumZeroed( program.m_parameters.Num() );
            SetDefaultState(currentParams);

            // TODO: we can optimize by precalculating what ops have the required optype below
            m_opType = opType;
            m_constant = constant;

            m_allParams.SetNumZeroed( program.m_parameters.Num() );
            FullTraverse( program );

            for (int32 i=0; i<m_allParams.Num(); ++i )
            {
                if ( m_allParams[i] )
                {
                    m_sortedParams.Add( int(i) );
                }
            }
        }


        virtual bool Visit( OP::ADDRESS at, FProgram& program )
        {
            bool recurse = true;

            if ( m_constSearch.Run( at ) )
            {
                switch ( program.GetOpType(at) )
                {
                case OP_TYPE::NU_CONDITIONAL:
                case OP_TYPE::SC_CONDITIONAL:
                case OP_TYPE::CO_CONDITIONAL:
                case OP_TYPE::IM_CONDITIONAL:
                case OP_TYPE::ME_CONDITIONAL:
                case OP_TYPE::LA_CONDITIONAL:
                case OP_TYPE::IN_CONDITIONAL:
				case OP_TYPE::ED_CONDITIONAL:
                {
					OP::ConditionalArgs args = program.GetOpArgs<OP::ConditionalArgs>(at);

                    // If the constant is present in only one of the 2 branches
                    bool foundYes = m_constSearch.Run( args.yes  );
                    bool foundNo = m_constSearch.Run( args.no );

                    if ( foundYes!=foundNo )
                    {
                        m_conditionVisitor.Run( args.condition, program );

                        TArray<int32> currentParams = GetCurrentState();
                        for (int32 p=0; p<m_conditionVisitor.RelevantParams.Num(); ++p )
                        {
                            currentParams[ m_conditionVisitor.RelevantParams[p] ]++;
                        }

                        // Set state for child recursion
                        SetCurrentState( currentParams );
                    }

                    break;
                }

                case OP_TYPE::NU_SWITCH:
                case OP_TYPE::SC_SWITCH:
                case OP_TYPE::CO_SWITCH:
                case OP_TYPE::IM_SWITCH:
                case OP_TYPE::ME_SWITCH:
                case OP_TYPE::LA_SWITCH:
				case OP_TYPE::ED_SWITCH:
                {
					const uint8* data = program.GetOpArgsPointer(at);
					
					OP::ADDRESS VarAddress;
					FMemory::Memcpy( &VarAddress, data, sizeof(OP::ADDRESS) );

                    m_conditionVisitor.Run( VarAddress, program );

					TArray<int32> currentParams = GetCurrentState();
                    for (int32 p=0; p<m_conditionVisitor.RelevantParams.Num(); ++p )
                    {
                        currentParams[ m_conditionVisitor.RelevantParams[p] ]++;
                    }

                    // Set state for child recursion
                    SetCurrentState( currentParams );

                    break;
                }

                default:
                {
                    // If we hit the constant we are analysing
                    if ( m_opType == OP_TYPE::ME_CONSTANT
                         &&
                         program.GetOpType(at) == m_opType )
                    {
						OP::MeshConstantArgs args = program.GetOpArgs<OP::MeshConstantArgs>(at);
                        if ( args.value == m_constant )
                        {
                            // Accumulate the currently relevant parameters
                            const TArray<int>& currentParams = GetCurrentState();
                            for (int32 i=0; i<currentParams.Num(); ++i )
                            {
                                m_allParams[i] += currentParams[i];
                            }
                        }
                    }
                    else if ( program.GetOpType(at) == m_opType )
                    {
						OP::ResourceConstantArgs args = program.GetOpArgs<OP::ResourceConstantArgs>(at);
                        if ( args.value == m_constant )
                        {
                            // Accumulate the currently relevant parameters
                            const TArray<int>& currentParams = GetCurrentState();
                            for (int32 i=0; i<currentParams.Num(); ++i )
                            {
                                m_allParams[i] += currentParams[i];
                            }
                        }
                    }

                    break;
                }

                }

            }

            return recurse;
        }

    public:

        //! Output result
		TArray<int> m_sortedParams;

    private:

        OP::ADDRESS m_constant;
        OP_TYPE m_opType;

		TArray<int> m_allParams;

        SubtreeParametersVisitor m_conditionVisitor;
        SubtreeSearchConstantVisitor m_constSearch;

    };


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class AccumulateImageFormatsAST : public Visitor_TopDown_Unique_Const< TArray<bool> >
    {
    public:

        void Run( const ASTOpList& roots )
        {
            MUTABLE_CPUPROFILER_SCOPE(AccumulateImageFormatsAST);

            // TODO: Use statically sized vectors (std::array)
            //vector<bool> initial( IF_COUNT, true );
            //std::fill( m_supportedFormats.begin(), m_supportedFormats.end(), initial );

			TArray<bool> defaultState;
			defaultState.SetNumZeroed(int32(EImageFormat::IF_COUNT));

            Traverse( roots, defaultState );
        }


        bool Visit( const Ptr<ASTOp>& node ) override
        {
            bool recurse = true;

            const TArray<bool>& currentFormats = GetCurrentState();

			TArray<bool> defaultState;
			defaultState.SetNumZeroed(int32(EImageFormat::IF_COUNT));
			bool allFalse = currentFormats == defaultState;

            // Can we use the cache?
            if (allFalse)
            {
                if (m_visited.count(node))
                {
                    return false;
                }

                m_visited.insert(node);
            }

            switch ( node->GetOpType() )
            {
            case OP_TYPE::IM_CONSTANT:
            {
                // Remove unsupported formats
				const ASTOpConstantResource* op = static_cast<const ASTOpConstantResource*>(node.get());
                if (!m_supportedFormats.count(op))
                {
					TArray<bool> initial;
					initial.Init( true, int32(EImageFormat::IF_COUNT) );
                    m_supportedFormats.insert( std::make_pair(op, std::move(initial)) );
                }

                for ( unsigned f=0; f< unsigned(EImageFormat::IF_COUNT); ++f )
                {
                    if ( !currentFormats[f] )
                    {
                        m_supportedFormats[op][f] = false;
                    }
                }
                recurse = false;
                break;
            }

            case OP_TYPE::IM_SWITCH:
            case OP_TYPE::IM_CONDITIONAL:
                // Switches and conditionals don't change the supported formats
                break;

            case OP_TYPE::IM_COMPOSE:
            {
                recurse = false;
				const ASTOpImageCompose* op = static_cast<const ASTOpImageCompose*>(node.get());

				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
                RecurseWithState( op->Layout.child(), newState );
                RecurseWithState( op->Base.child(), newState );
                RecurseWithState( op->BlockImage.child(), newState );

                if ( op->Mask )
                {
                    newState[(int32)EImageFormat::IF_L_UBIT_RLE] = true;
                    RecurseWithState( op->Mask.child(), newState );
                }
                break;
            }

            case OP_TYPE::IM_LAYERCOLOUR:
            {
                recurse = false;
				const ASTOpImageLayerColor* op = static_cast<const ASTOpImageLayerColor*>(node.get());
				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
				RecurseWithState( op->base.child(), newState );
                RecurseWithState( op->color.child(), newState );

                if ( op->mask )
                {
                    newState[(int32)EImageFormat::IF_L_UBYTE] = true;
                    newState[(int32)EImageFormat::IF_L_UBYTE_RLE] = true;

                    RecurseWithState( op->mask.child(), newState );
                }
                break;
            }

            case OP_TYPE::IM_LAYER:
            {
                recurse = false;
				const ASTOpImageLayer* op = static_cast<const ASTOpImageLayer*>(node.get());
				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
				RecurseWithState( op->base.child(), newState );
                RecurseWithState( op->blend.child(), newState );

                if (op->mask)
                {
                    newState[(int32)EImageFormat::IF_L_UBYTE] = true;
                    newState[(int32)EImageFormat::IF_L_UBYTE_RLE] = true;

                    RecurseWithState( op->mask.child(), newState );
                }
                break;
            }

            case OP_TYPE::IM_MULTILAYER:
            {
                recurse = false;
				const ASTOpImageMultiLayer* op = static_cast<const ASTOpImageMultiLayer*>(node.get());
				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
				RecurseWithState( op->base.child(), newState );
                RecurseWithState( op->blend.child(), newState );

                if (op->mask)
                {
                    newState[(size_t)EImageFormat::IF_L_UBYTE] = true;
                    newState[(size_t)EImageFormat::IF_L_UBYTE_RLE] = true;

                    RecurseWithState( op->mask.child(), newState );
                }
                break;
            }

            case OP_TYPE::IM_DISPLACE:
            {
                recurse = false;
				const ASTOpFixed* op = static_cast<const ASTOpFixed*>(node.get());
				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
				RecurseWithState( op->children[op->op.args.ImageDisplace.source].child(), newState );

                newState[(int32)EImageFormat::IF_L_UBYTE ] = true;
                newState[(int32)EImageFormat::IF_L_UBYTE_RLE ] = true;

                RecurseWithState( op->children[op->op.args.ImageDisplace.displacementMap].child(), newState );
                break;
            }

            default:
            {
                //m_currentFormats.Add(vector<bool>(IF_COUNT, false));
                //Recurse(at, program);
                //m_currentFormats.pop_back();

				TArray<bool> newState;
				newState.Init(false, int32(EImageFormat::IF_COUNT));
				if (currentFormats != newState)
                {
                    RecurseWithState(node, newState);
                    recurse = false;
                }
                else
                {
                    recurse = true;
                }
                break;
            }

            }

            return recurse;
        }

    public:

        //! Result of this visitor:
        //! Formats known to be supported by every constant image.
        std::unordered_map< Ptr<const ASTOpConstantResource>, TArray<bool> > m_supportedFormats;

    private:

        //! Cache. Only valid is current formats are all false.
        std::unordered_set<Ptr<ASTOp>> m_visited;

    };




    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class AccumulateMeshChannelUsageAST : public Visitor_TopDown_Unique_Const< uint64_t >
    {
    public:

        void Run( const ASTOpList& roots )
        {
            MUTABLE_CPUPROFILER_SCOPE(AccumulateMeshChannelUsageAST);

            // Sanity check in case we add more semantics
            static_assert(MBS_COUNT<sizeof(uint64)*8, "Too many mesh buffer semantics." );

            // Default state: we need everything except internal semantics
            uint64 defaultState = 0xffffffffffffffff;
            defaultState ^= (UINT64_C(1)<<MBS_LAYOUTBLOCK);
            defaultState ^= (UINT64_C(1)<<MBS_VERTEXINDEX);

            Traverse(roots,defaultState);
        }


        bool Visit( const Ptr<ASTOp>& node ) override
        {
            bool recurse = true;

            uint64 currentSemantics = GetCurrentState();

            switch ( node->GetOpType() )
            {

            case OP_TYPE::ME_CONSTANT:
            {
                // Accumulate necessary semantics
				const ASTOpConstantResource* op = static_cast<const ASTOpConstantResource*>(node.get());
                uint64 currentFlags = 0;
                if (m_requiredSemantics.count(op))
                {
                    currentFlags = m_requiredSemantics[op];
                }
                else
                {
                    m_requiredSemantics.insert(std::make_pair(op,currentFlags));
                }

                currentFlags |= currentSemantics;
                m_requiredSemantics[op] = currentFlags;
                recurse = false;
                break;
            }

            // TODO: These could probably optimise something
            //case OP_TYPE::IM_RASTERMESH: break;

            case OP_TYPE::ME_DIFFERENCE:
            {
                recurse = false;

				const ASTOpMeshDifference* op = static_cast<const ASTOpMeshDifference*>(node.get());

                uint64 newState = currentSemantics;
                newState |= (UINT64_C(1)<<MBS_VERTEXINDEX);
                RecurseWithState( op->Base.child(), newState );

                RecurseWithState( op->Target.child(), currentSemantics );
                break;
             }

            case OP_TYPE::ME_REMOVEMASK:
            {
                recurse = false;

				const ASTOpMeshRemoveMask* op = static_cast<const ASTOpMeshRemoveMask*>(node.get());

                uint64 newState = currentSemantics;
                newState |= (UINT64_C(1)<<MBS_VERTEXINDEX);
                RecurseWithState( op->source.child(), newState );
                for( const TPair<ASTChild, ASTChild>& r: op->removes )
                {
                    RecurseWithState( r.Value.child(), newState );
                }
                break;
             }

            case OP_TYPE::ME_MORPH:
            {
                recurse = false;

				const ASTOpMeshMorph* op = static_cast<const ASTOpMeshMorph*>(node.get());

                uint64 newState = currentSemantics;
                newState |= (UINT64_C(1)<<MBS_VERTEXINDEX);
                RecurseWithState( op->Base.child(), newState );
                RecurseWithState( op->Target.child(), newState );
                break;
             }


            case OP_TYPE::ME_APPLYLAYOUT:
            {
                recurse = false;

				const ASTOpFixed* op = static_cast<const ASTOpFixed*>(node.get());

                uint64 newState = currentSemantics;
                newState |= (UINT64_C(1)<<MBS_LAYOUTBLOCK);
                RecurseWithState( op->children[op->op.args.MeshApplyLayout.mesh].child(), newState );

                RecurseWithState( op->children[op->op.args.MeshApplyLayout.layout].child(), currentSemantics );
                break;
            }

            case OP_TYPE::ME_EXTRACTLAYOUTBLOCK:
            {
                recurse = false;

				const ASTOpMeshExtractLayoutBlocks* op = static_cast<const ASTOpMeshExtractLayoutBlocks*>(node.get());

                // todo: check if we really need all of them
                uint64 newState = currentSemantics;
                newState |= (UINT64_C(1)<<MBS_LAYOUTBLOCK);
                newState |= (UINT64_C(1)<<MBS_VERTEXINDEX);

                RecurseWithState( op->Source.child(), newState );
                break;
            }

			case OP_TYPE::LA_FROMMESH:
			{
				recurse = false;

				const ASTOpLayoutFromMesh* op = static_cast<const ASTOpLayoutFromMesh*>(node.get());

				uint64 newState = currentSemantics;
				newState |= (UINT64_C(1) << MBS_LAYOUTBLOCK);

				RecurseWithState(op->Mesh.child(), newState);
				break;
			}

            case OP_TYPE::IN_ADDMESH:
            {
                recurse = false;

				const ASTOpInstanceAdd* op = static_cast<const ASTOpInstanceAdd*>(node.get());

                RecurseWithState( op->instance.child(), currentSemantics );

                uint64 newState = GetDefaultState();
                RecurseWithState( op->value.child(), newState );
                break;
            }

            default:
                // Unhandled op, we may need everything? Recurse with current state?
                //uint64 newState = 0xffffffffffffffff;
                break;

            }

            return recurse;
        }

    public:

        //! Result of this visitor:
        //! Used mesh channel semantics for each constant mesh
        std::unordered_map< Ptr<const ASTOpConstantResource>, uint64 > m_requiredSemantics;

    };


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    // Todo: move to its own file
    inline void MeshRemoveUnusedBufferSemantics( Mesh* pMesh, uint64 usedSemantics )
    {
        // right now we only remove entire buffers if no channel is used
        // TODO: remove from inside the buffer?
        for (int v=0; v<pMesh->GetVertexBuffers().GetBufferCount(); )
        {
            bool used = false;
            for (int c=0; !used && c<pMesh->GetVertexBuffers().GetBufferChannelCount(v); ++c)
            {
                MESH_BUFFER_SEMANTIC semantic;
                pMesh->GetVertexBuffers().GetChannel(v,c,&semantic,
                                                      nullptr, nullptr, nullptr, nullptr);
                used = (( (UINT64_C(1)<<semantic) ) & usedSemantics) != 0;
            }

            if (!used)
            {
                TArray<MESH_BUFFER>& buffers = pMesh->GetVertexBuffers().m_buffers;
                buffers.RemoveAt(v);
            }
            else
            {
                ++v;
            }
        }

        // TODO: hack, if we don't need layouts, remove them.
        {
            uint64 layoutSemantics = 0;
            layoutSemantics |= (UINT64_C(1)<<MBS_LAYOUTBLOCK);

            if ( (usedSemantics&layoutSemantics) == 0)
            {
                pMesh->m_layouts.Empty();
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    void DataOptimise( const CompilerOptions* Options, ASTOpList& roots )
    {
		int32 ImageCompressionQuality = Options->GetPrivate()->ImageCompressionQuality;
		const FModelOptimizationOptions& OptimizeOptions = Options->GetPrivate()->OptimisationOptions;

        // Images
        AccumulateImageFormatsAST accFormat;
        accFormat.Run( roots );

        // See if we can convert some constants to more efficient formats
        ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, [&](Ptr<ASTOp>& n)
        {
            if (n->GetOpType()==OP_TYPE::IM_CONSTANT)
            {
				ASTOpConstantResource* typed = static_cast<ASTOpConstantResource*>(n.get());
                Ptr<const Image> pOld = static_cast<const Image*>(typed->GetValue().get());

				FImageOperator ImOp = FImageOperator::GetDefault( Options->GetPrivate()->ImageFormatFunc );

				// See if there is a better format for this image
				FVector4f PlainColor;
				if ( pOld->IsPlainColour(PlainColor) )
				{
					// It is more efficient to just have an instruction for it instead, to avoid the overhead
					// of data loading. 
					// Warning This eliminates the mips. \TODO: Add support for mips in plaincolour instruction?
					Ptr<ASTOpFixed> NewColor = new ASTOpFixed;
					NewColor->op.type = OP_TYPE::CO_CONSTANT;
					NewColor->op.args.ColourConstant.value[0] = PlainColor[0];
					NewColor->op.args.ColourConstant.value[1] = PlainColor[1];
					NewColor->op.args.ColourConstant.value[2] = PlainColor[2];
					NewColor->op.args.ColourConstant.value[3] = PlainColor[3];

					Ptr<ASTOpFixed> NewPlain = new ASTOpFixed;
					NewPlain->op.type = OP_TYPE::IM_PLAINCOLOUR;
					NewPlain->SetChild( NewPlain->op.args.ImagePlainColour.colour, NewColor );
					NewPlain->op.args.ImagePlainColour.format = pOld->GetFormat();
					NewPlain->op.args.ImagePlainColour.size[0] = pOld->GetSizeX();
					NewPlain->op.args.ImagePlainColour.size[1] = pOld->GetSizeY();
					NewPlain->op.args.ImagePlainColour.LODs = 1;

					ASTOp::Replace(n, NewPlain);

				}
				else if ( accFormat.m_supportedFormats[typed][(int32)EImageFormat::IF_L_UBIT_RLE] )
                {
                    ImagePtr pNew = ImOp.ImagePixelFormat( ImageCompressionQuality, pOld.get(), EImageFormat::IF_L_UBIT_RLE );

                    // Only replace if the compression was worth!
                    int32 oldSize = pOld->GetDataSize();
					int32 newSize = pNew->GetDataSize();
                    if (float(oldSize) > float(newSize) * OptimizeOptions.MinRLECompressionGain)
                    {
                        typed->SetValue(pNew, OptimizeOptions.DiskCacheContext);
                    }
                }
                else if ( accFormat.m_supportedFormats[typed][(int32)EImageFormat::IF_L_UBYTE_RLE] )
                {
                    ImagePtr pNew = ImOp.ImagePixelFormat( ImageCompressionQuality, pOld.get(), EImageFormat::IF_L_UBYTE_RLE );

                    // Only replace if the compression was worth!
					int32 oldSize = pOld->GetDataSize();
					int32 newSize = pNew->GetDataSize();
                    if (float(oldSize) > float(newSize) * OptimizeOptions.MinRLECompressionGain)
                    {
                        typed->SetValue(pNew, OptimizeOptions.DiskCacheContext);
                    }
                }
            }
        });


        // Meshes
        AccumulateMeshChannelUsageAST meshSemanticsVisitor;
        meshSemanticsVisitor.Run( roots );

        // See if we can remove some buffers from the constants
        ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, [&](Ptr<ASTOp>& n)
        {
            if (n->GetOpType()==OP_TYPE::ME_CONSTANT)
            {
				ASTOpConstantResource* typed = static_cast<ASTOpConstantResource*>(n.get());
                Ptr<Mesh> pMesh = static_cast<const Mesh*>(typed->GetValue().get())->Clone();
                MeshRemoveUnusedBufferSemantics( pMesh.get(), meshSemanticsVisitor.m_requiredSemantics[typed]);
                typed->SetValue(pMesh, OptimizeOptions.DiskCacheContext);
            }
        });

    }


}
