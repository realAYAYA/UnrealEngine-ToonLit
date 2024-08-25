// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeImageFormatPrivate.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageMakeGrowMap.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageTransform.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/Table.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    bool SemanticOptimiserAST(
		ASTOpList& roots,
		const FModelOptimizationOptions& optimisationOptions,
		int32 Pass
	)
    {
        MUTABLE_CPUPROFILER_SCOPE(SemanticOptimiserAST);

        bool modified = false;

        // TODO: isn't top down better suited?
        ASTOp::Traverse_BottomUp_Unique( roots, [&](Ptr<ASTOp>& n)
        {
            auto o = n->OptimiseSemantic(optimisationOptions, Pass);

            // If the returned value is null it means no change.
            if (o && o!=n)
            {
                modified = true;
                ASTOp::Replace(n,o);
            }
        });

        return modified;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> ASTOpFixed::OptimiseSemantic( const FModelOptimizationOptions& options, int32 Pass) const
    {
        Ptr<ASTOp> at;

        OP_TYPE type = GetOpType();
        switch ( type )
        {

        case OP_TYPE::BO_NOT:
        {
			Ptr<ASTOp> sourceAt = children[op.args.BoolNot.source].child();
            if  ( sourceAt->GetOpType() == OP_TYPE::BO_CONSTANT )
            {
                Ptr<ASTOpConstantBool> newAt = new ASTOpConstantBool();
                newAt->value = !newAt->value;
                at=newAt;
            }
            break;
        }

        case OP_TYPE::BO_AND:
        {
            bool changed = false;
			Ptr<ASTOp> aAt = children[op.args.BoolBinary.a].child();
			Ptr<ASTOp> bAt = children[op.args.BoolBinary.b].child();
            if ( !aAt )
            {
                at = bAt;
                changed = true;
            }
            else if ( !bAt )
            {
                at = aAt;
                changed = true;
            }
            else if ( aAt->GetOpType() == OP_TYPE::BO_CONSTANT )
            {
                if (static_cast<const ASTOpConstantBool*>(aAt.get())->value)
                {
                    at = bAt;
                    changed = true;
                }
                else
                {
                    at = aAt;
                    changed = true;
                }
            }
            else if ( bAt->GetOpType() == OP_TYPE::BO_CONSTANT )
            {
                if (static_cast<const ASTOpConstantBool*>(bAt.get())->value)
                {
                    at = aAt;
                    changed = true;
                }
                else
                {
                    at = bAt;
                    changed = true;
                }
            }

            // Common cases of repeated branch in children
            else if ( aAt->GetOpType() == type )
            {
				const ASTOpFixed* typedA = static_cast<const ASTOpFixed*>(aAt.get());
                if ( typedA->children[typedA->op.args.BoolBinary.a].child()==bAt
                     ||
                     typedA->children[typedA->op.args.BoolBinary.b].child()==bAt )
                {
                    at = aAt;
                    changed = true;
                }
            }
            else if ( bAt->GetOpType() == type )
            {
				const ASTOpFixed* typedB = static_cast<const ASTOpFixed*>(bAt.get());
                if ( typedB->children[typedB->op.args.BoolBinary.b].child()==aAt
                     ||
                     typedB->children[typedB->op.args.BoolBinary.b].child()==bAt )
                {
                    at = bAt;
                    changed = true;
                }
            }

            else if (aAt == bAt || *aAt==*bAt)
            {
                at = aAt;
                changed = true;
            }

            // if it became null, it means true (neutral AND argument)
            if (changed && !at)
            {
                Ptr<ASTOpConstantBool> newAt = new ASTOpConstantBool();
                newAt->value = true;
                at = newAt;
            }
            break;
        }

        case OP_TYPE::BO_OR:
        {
            bool changed = false;
			Ptr<ASTOp> aAt = children[op.args.BoolBinary.a].child();
			Ptr<ASTOp> bAt = children[op.args.BoolBinary.b].child();
            if ( !aAt )
            {
                at = bAt;
                changed = true;
            }
            else if ( !bAt )
            {
                at = aAt;
                changed = true;
            }
            else if ( aAt->GetOpType() == OP_TYPE::BO_CONSTANT )
            {
                if (static_cast<const ASTOpConstantBool*>(aAt.get())->value)
                {
                    at = aAt;
                    changed = true;
                }
                else
                {
                    at = bAt;
                    changed = true;
                }
            }
            else if ( bAt->GetOpType() == OP_TYPE::BO_CONSTANT )
            {
                if (static_cast<const ASTOpConstantBool*>(bAt.get())->value)
                {
                    at = bAt;
                    changed = true;
                }
                else
                {
                    at = aAt;
                    changed = true;
                }
            }

            // Common cases of repeated branch in children
            else if ( aAt->GetOpType() == type )
            {
				const ASTOpFixed* typedA = static_cast<const ASTOpFixed*>(aAt.get());
                if ( typedA->children[typedA->op.args.BoolBinary.a].child()==bAt
                     ||
                     typedA->children[typedA->op.args.BoolBinary.b].child()==bAt )
                {
                    at = aAt;
                    changed = true;
                }
            }
            else if ( bAt->GetOpType() == type )
            {
				const ASTOpFixed* typedB = static_cast<const ASTOpFixed*>(bAt.get());
                if ( typedB->children[typedB->op.args.BoolBinary.b].child()==aAt
                     ||
                     typedB->children[typedB->op.args.BoolBinary.b].child()==bAt )
                {
                    at = bAt;
                    changed = true;
                }
            }

            else if (aAt == bAt || *aAt==*bAt)
            {
                at = aAt;
                changed = true;
            }

            // if it became null, it means false (neutral OR argument)
            if (changed && !at)
            {
                Ptr<ASTOpConstantBool> newAt = new ASTOpConstantBool();
                newAt->value = false;
                at = newAt;
            }

            break;
        }

		case OP_TYPE::CO_SWIZZLE:
		{
			// Optimizations that can be applied per-channel
			{
				Ptr<ASTOpFixed> NewAt;

				for (int32 ChannelIndex = 0; ChannelIndex < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++ChannelIndex)
				{
					Ptr<ASTOp> Candidate = children[op.args.ColourSwizzle.sources[ChannelIndex]].child();
					if (!Candidate)
					{
						continue;
					}

					switch (Candidate->GetOpType())
					{

						// Swizzle + swizzle = swizzle
					case OP_TYPE::CO_SWIZZLE:
					{
						if (!NewAt)
						{
							NewAt = mu::Clone<ASTOpFixed>(this);
						}
						const ASTOpFixed* TypedCandidate = static_cast<const ASTOpFixed*>(Candidate.get());
						int32 CandidateChannel = op.args.ColourSwizzle.sourceChannels[ChannelIndex];

						NewAt->SetChild(NewAt->op.args.ColourSwizzle.sources[ChannelIndex], TypedCandidate->children[TypedCandidate->op.args.ColourSwizzle.sources[CandidateChannel]]);
						NewAt->op.args.ColourSwizzle.sourceChannels[ChannelIndex] = TypedCandidate->op.args.ColourSwizzle.sourceChannels[CandidateChannel];
						break;
					}

					default:
						break;

					}
				}

				at = NewAt;
			}

			// Not optimized yet?
			if (!at)
			{
				// Optimizations that depend on all channels.
				bool bAllChannelsSameType = true;
				OP_TYPE AllChannelsType = OP_TYPE::NONE;
				for (int32 ChannelIndex = 0; ChannelIndex < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++ChannelIndex)
				{
					Ptr<ASTOp> Candidate = children[op.args.ColourSwizzle.sources[ChannelIndex]].child();
					if (!Candidate)
					{
						continue;
					}

					if (ChannelIndex == 0)
					{
						AllChannelsType = Candidate->GetOpType();
					}
					else if (Candidate->GetOpType()!=AllChannelsType)
					{
						bAllChannelsSameType = false;
						break;
					}
				}

				if (bAllChannelsSameType)
				{
					switch (AllChannelsType)
					{
					case OP_TYPE::CO_FROMSCALARS:
					{
						// We can remove the swizzle and replace it with a new FromScalars actually swizzling the inputs.

						Ptr<ASTOpFixed> NewAt = new ASTOpFixed();
						NewAt->op.type = OP_TYPE::CO_FROMSCALARS;

						for (int32 ChannelIndex = 0; ChannelIndex < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++ChannelIndex)
						{
							const ASTOp* SelectedSourceGeneric = children[op.args.ColourSwizzle.sources[ChannelIndex]].child().get();
							if (SelectedSourceGeneric)
							{
								const ASTOpFixed* SelectedSource = static_cast<const ASTOpFixed*>(SelectedSourceGeneric);
								int32 SelectedChannel = op.args.ColourSwizzle.sourceChannels[ChannelIndex];
								Ptr<ASTOp> SelectedFloatInput = SelectedSource->children[SelectedSource->op.args.ColourFromScalars.v[SelectedChannel]].child();
								NewAt->SetChild(NewAt->op.args.ColourFromScalars.v[ChannelIndex], SelectedFloatInput);
							}
						}

						at = NewAt;
						break;
					}

					default: break;
					}
				}
			}
			
			break;
		}

		//-------------------------------------------------------------------------------------
		case OP_TYPE::IM_RESIZEREL:
		{
			Ptr<ASTOp> sourceAt = children[op.args.ImageResizeRel.source].child();

			// The instruction can be sunk
			OP_TYPE sourceType = sourceAt->GetOpType();
			switch (sourceType)
			{
			// This is done here instead of in the OptimizeSize step to prevent removing 
			// resize_rel too early.
			case OP_TYPE::IM_RESIZE:
			{
				Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(sourceAt.get());
				NewOp->op.args.ImageResize.size[0] = int16(NewOp->op.args.ImageResize.size[0] * op.args.ImageResizeRel.factor[0]);
				NewOp->op.args.ImageResize.size[1] = int16(NewOp->op.args.ImageResize.size[1] * op.args.ImageResizeRel.factor[1]);

				at = NewOp;
				break;
			}


			default:
				break;

			}

			break;
		}

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_PIXELFORMAT:
		case OP_TYPE::IM_SWIZZLE:
		case OP_TYPE::IM_LAYER:
		{
			check(false); // moved to its own operation class
			break;
		}

		//-------------------------------------------------------------------------------------
		case OP_TYPE::ME_MASKDIFF:
		{
			Ptr<ASTOp> Fragment = children[op.args.MeshMaskDiff.fragment].child();
			OP_TYPE FragmentType = Fragment->GetOpType();
			switch (FragmentType)
			{
			case OP_TYPE::ME_ADDTAGS:
			{
				// Tags in the fragment can be ignored.
				const ASTOpMeshAddTags* Add = static_cast<const ASTOpMeshAddTags*>(Fragment.get());

				Ptr<ASTOpFixed> NewAt = mu::Clone<ASTOpFixed>(this);
				NewAt->SetChild(NewAt->op.args.MeshMaskDiff.fragment, Add->Source);
				at = NewAt;
				break;
			}

			default:
				break;
			}

			break;
		}

		//-------------------------------------------------------------------------------------
		case OP_TYPE::ME_APPLYLAYOUT:
		{
			Ptr<ASTOp> Base = children[op.args.MeshApplyLayout.mesh].child();
			OP_TYPE BaseType = Base->GetOpType();
			switch (BaseType)
			{
			case OP_TYPE::ME_ADDTAGS:
			{
				// Add the tags after layout
				Ptr<ASTOpMeshAddTags> NewAddTags = mu::Clone<ASTOpMeshAddTags>(Base);

				if (NewAddTags->Source)
				{
					Ptr<ASTOpFixed> NewAt = mu::Clone<ASTOpFixed>(this);
					NewAt->SetChild(NewAt->op.args.MeshApplyLayout.mesh, NewAddTags->Source);
					NewAddTags->Source = NewAt;
				}

				at = NewAddTags;
				break;
			}

			default:
				break;
			}

			break;
		}

        default:
            break;
        }

        return at;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    bool SinkOptimiserAST
    (
            ASTOpList& InRoots,
            const FModelOptimizationOptions& InOptimisationOptions
    )
    {
        MUTABLE_CPUPROFILER_SCOPE(SinkOptimiserAST);

        bool bModified = false;

		FOptimizeSinkContext Context;

        ASTOp::Traverse_TopDown_Unique_Imprecise(InRoots, [&](Ptr<ASTOp>& n)
        {
            auto o = n->OptimiseSink(InOptimisationOptions, Context);
            if (o && n!=o)
            {
				bModified = true;
                ASTOp::Replace(n,o);
            }

            return true;
        });

        return bModified;
    }


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpFixed::OptimiseSink( const FModelOptimizationOptions& options, FOptimizeSinkContext& context ) const
    {
        Ptr<ASTOp> at;

        OP_TYPE type = GetOpType();
        switch ( type )
        {

        case OP_TYPE::ME_PROJECT:
        {
            auto sourceAt = children[op.args.MeshProject.mesh].child();
            if (!sourceAt) { break; }

            auto projectorAt = children[op.args.MeshProject.projector].child();
            if (!projectorAt) { break; }

            OP_TYPE sourceType = sourceAt->GetOpType();
            switch ( sourceType )
            {

            case OP_TYPE::ME_CONDITIONAL:
            {
                if (projectorAt->GetOpType()==OP_TYPE::PR_CONSTANT)
                {
                    // We move the project down the two paths
                    auto nop = mu::Clone<ASTOpConditional>(sourceAt);

                    auto aOp = mu::Clone<ASTOpFixed>(this);
                    aOp->SetChild( aOp->op.args.MeshProject.mesh, nop->yes );
                    nop->yes = aOp;

                    auto bOp = mu::Clone<ASTOpFixed>(this);
                    bOp->SetChild( bOp->op.args.MeshProject.mesh, nop->no);
                    nop->no = bOp;

                    at = nop;
                }
                break;
            }

            case OP_TYPE::ME_SWITCH:
            {
                if (projectorAt->GetOpType()==OP_TYPE::PR_CONSTANT)
                {
                    // Move the format down all the paths
                    auto nop = mu::Clone<ASTOpSwitch>(sourceAt);

                    if (nop->def)
                    {
                        auto defOp = mu::Clone<ASTOpFixed>(this);
                        defOp->SetChild( defOp->op.args.MeshProject.mesh, nop->def );
                        nop->def = defOp;
                    }

                    // We need to copy the options because we change them
                    for ( int32 v=0; v<nop->cases.Num(); ++v )
                    {
                        if ( nop->cases[v].branch )
                        {
                            auto bOp = mu::Clone<ASTOpFixed>(this);
                            bOp->SetChild( bOp->op.args.MeshProject.mesh, nop->cases[v].branch );
                            nop->cases[v].branch = bOp;
                        }
                    }

                    at = nop;
                }
                break;
            }

			case OP_TYPE::ME_ADDTAGS:
			{
				// Add the tags after layout
				Ptr<ASTOpMeshAddTags> NewAddTags = mu::Clone<ASTOpMeshAddTags>(sourceAt);

				if (NewAddTags->Source)
				{
					Ptr<ASTOpFixed> NewAt = mu::Clone<ASTOpFixed>(this);
					NewAt->SetChild(NewAt->op.args.MeshProject.mesh, NewAddTags->Source);
					NewAddTags->Source = NewAt;
				}

				at = NewAddTags;
				break;
			}

            default:
                break;
            }

            break;
        }


        //-----------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------
		case OP_TYPE::IM_SWIZZLE:
		case OP_TYPE::LA_REMOVEBLOCKS:
		case OP_TYPE::IM_RASTERMESH:
		{
			// Moved to their own operation class
			check(false);
            break;
        }


        //-----------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------
        case OP_TYPE::IM_DISPLACE:
        {
			Ptr<ASTOp> OriginalAt = at;
			Ptr<ASTOp> sourceAt = children[op.args.ImageDisplace.source].child();
			Ptr<ASTOp> displaceMapAt = children[op.args.ImageDisplace.displacementMap].child();

            switch ( sourceAt->GetOpType() )
            {

            case OP_TYPE::IM_CONDITIONAL:
            {
                if (displaceMapAt->GetOpType()==OP_TYPE::IM_CONDITIONAL)
                {
					const ASTOpConditional* typedSource = static_cast<const ASTOpConditional*>(sourceAt.get());
					const ASTOpConditional* typedDisplacementMap = static_cast<const ASTOpConditional*>(displaceMapAt.get());

                    if (typedSource->condition==typedDisplacementMap->condition)
                    {
                        auto nop = mu::Clone<ASTOpConditional>(sourceAt);

                        auto aOp = mu::Clone<ASTOpFixed>(this);
                        aOp->SetChild( aOp->op.args.ImageDisplace.source, typedSource->yes );
                        aOp->SetChild( aOp->op.args.ImageDisplace.displacementMap, typedDisplacementMap->yes );
                        nop->yes = aOp;

                        auto bOp = mu::Clone<ASTOpFixed>(this);
                        bOp->SetChild( bOp->op.args.ImageDisplace.source, typedSource->no);
                        bOp->SetChild( bOp->op.args.ImageDisplace.displacementMap, typedDisplacementMap->no);
                        nop->no = bOp;

                        at = nop;
                    }
                }
                break;
            }

            case OP_TYPE::IM_SWITCH:
            {
                if (displaceMapAt->GetOpType()==OP_TYPE::IM_SWITCH)
                {
					const ASTOpSwitch* typedSource = static_cast<const ASTOpSwitch*>(sourceAt.get());
					const ASTOpSwitch* typedDisplacementMap = static_cast<const ASTOpSwitch*>(displaceMapAt.get());

                    if (typedSource->IsCompatibleWith(typedDisplacementMap))
                    {
                        // Move the format down all the paths
                        auto nop = mu::Clone<ASTOpSwitch>(sourceAt);

                        if (nop->def)
                        {
							Ptr<ASTOpFixed> defOp = mu::Clone<ASTOpFixed>(this);
                            defOp->SetChild( defOp->op.args.ImageDisplace.source, typedSource->def );
                            defOp->SetChild( defOp->op.args.ImageDisplace.displacementMap, typedDisplacementMap->def );
                            nop->def = defOp;
                        }

                        // We need to copy the options because we change them
                        for ( size_t v=0; v<nop->cases.Num(); ++v )
                        {
                            if ( nop->cases[v].branch )
                            {
								Ptr<ASTOpFixed> bOp = mu::Clone<ASTOpFixed>(this);
                                bOp->SetChild( bOp->op.args.ImageDisplace.source, typedSource->cases[v].branch );
                                bOp->SetChild( bOp->op.args.ImageDisplace.displacementMap, typedDisplacementMap->FindBranch(typedSource->cases[v].condition) );
                                nop->cases[v].branch = bOp;
                            }
                        }

                        at = nop;
                    }
                }

				// If we didn't optimize already, try to simply sink the source.
				if (OriginalAt == at)
				{
					// Make a project for every path
					auto nop = mu::Clone<ASTOpSwitch>(sourceAt.get());

					if (nop->def)
					{
						Ptr<ASTOpFixed> defOp = mu::Clone<ASTOpFixed>(this);
						defOp->SetChild(defOp->op.args.ImageDisplace.source, nop->def);
						nop->def = defOp;
					}

					// We need to copy the options because we change them
					for (size_t o = 0; o < nop->cases.Num(); ++o)
					{
						if (nop->cases[o].branch)
						{
							Ptr<ASTOpFixed> bOp = mu::Clone<ASTOpFixed>(this);
							bOp->SetChild(bOp->op.args.ImageDisplace.source, nop->cases[o].branch);
							nop->cases[o].branch = bOp;
						}
					}

					at = nop;
				}

                break;
            }

            default:
                break;
            }

            break;
        }


        default:
            break;
        }

        return at;
    }

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    bool SizeOptimiserAST( ASTOpList& roots )
    {
        MUTABLE_CPUPROFILER_SCOPE(SizeOptimiser);

        bool modified = false;

        // TODO: isn't top down better suited?
        ASTOp::Traverse_BottomUp_Unique( roots, [&](Ptr<ASTOp>& n)
        {
            auto o = n->OptimiseSize();
            if (o && o!=n)
            {
                modified = true;
                ASTOp::Replace(n,o);
            }
        });

        return modified;
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class Sink_ImageResizeRelAST
    {
    public:

        Ptr<ASTOp> Apply( const ASTOp* root )
        {
            m_root = root;
            OldToNew.Empty();

            check(root->GetOpType()==OP_TYPE::IM_RESIZEREL);

			const ASTOpFixed* typedRoot = static_cast<const ASTOpFixed*>(root);
            m_initialSource = typedRoot->children[typedRoot->op.args.ImageResizeRel.source].child();
            Ptr<ASTOp> newSource = Visit( m_initialSource, typedRoot );

            // If there is any change, it is the new root.
            if (newSource!=m_initialSource)
            {
                return newSource;
            }

            return nullptr;
        }

    protected:

        const ASTOp* m_root;
        Ptr<ASTOp> m_initialSource;
        //! For each operation we sink, the map from old instructions to new instructions.
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(Ptr<ASTOp> at, const ASTOpFixed* currentSinkingOp)
		{
			if (!at) return nullptr;

			// Already visited?
			const Ptr<ASTOp>* Cached = OldToNew.Find({ at,currentSinkingOp });
			if (Cached)
			{
				return *Cached;
			}

			float scaleX = currentSinkingOp->op.args.ImageResizeRel.factor[0];
			float scaleY = currentSinkingOp->op.args.ImageResizeRel.factor[1];

			Ptr<ASTOp> newAt = at;
			switch (at->GetOpType())
			{

			case OP_TYPE::IM_CONDITIONAL:
			{
				// We move the mask creation down the two paths
				auto newOp = mu::Clone<ASTOpConditional>(at);
				newOp->yes = Visit(newOp->yes.child(), currentSinkingOp);
				newOp->no = Visit(newOp->no.child(), currentSinkingOp);
				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_PIXELFORMAT:
			{
				// \todo: only if shrinking?
				Ptr<ASTOpImagePixelFormat> newOp = mu::Clone<ASTOpImagePixelFormat>(at);
				newOp->Source = Visit(newOp->Source.child(), currentSinkingOp);
				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_SWITCH:
			{
				// We move the mask creation down all the paths
				Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
				newOp->def = Visit(newOp->def.child(), currentSinkingOp);
				for (ASTOpSwitch::FCase& c : newOp->cases)
				{
					c.branch = Visit(c.branch.child(), currentSinkingOp);
				}
				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_SWIZZLE:
			{
				Ptr<ASTOpImageSwizzle> newOp = mu::Clone<ASTOpImageSwizzle>(at);
				for (int s = 0; s < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++s)
				{
					Ptr<ASTOp> channelOp = newOp->Sources[s].child();
					if (channelOp)
					{
						newOp->Sources[s] = Visit(channelOp, currentSinkingOp);
					}
				}
				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_COMPOSE:
			{
				// We can only optimise if the layout grid blocks size in pixels is
				// still an integer after relative scale
				bool acceptable = false;
				{
					const ASTOpImageCompose* typedAt = static_cast<const ASTOpImageCompose*>(at.get());
					Ptr<ASTOp> originalBaseOp = typedAt->Base.child();

					// \todo: recursion-proof cache?
					int layoutBlockPixelsX = 0;
					int layoutBlockPixelsY = 0;
					originalBaseOp->GetLayoutBlockSize(&layoutBlockPixelsX, &layoutBlockPixelsY);

					int scaledLayoutBlockPixelsX = int(layoutBlockPixelsX * scaleX);
					int scaledLayoutBlockPixelsY = int(layoutBlockPixelsY * scaleY);
					int unscaledLayoutBlockPixelsX = int(scaledLayoutBlockPixelsX / scaleX);
					int unscaledLayoutBlockPixelsY = int(scaledLayoutBlockPixelsY / scaleY);
					acceptable =
						(layoutBlockPixelsX != 0 && layoutBlockPixelsY != 0)
						&&
						(layoutBlockPixelsX == unscaledLayoutBlockPixelsX)
						&&
						(layoutBlockPixelsY == unscaledLayoutBlockPixelsY);
				}

				if (acceptable)
				{
					Ptr<ASTOpImageCompose> newOp = mu::Clone<ASTOpImageCompose>(at);

					Ptr<ASTOp> baseOp = newOp->Base.child();
					newOp->Base = Visit(baseOp, currentSinkingOp);

					Ptr<ASTOp> blockOp = newOp->BlockImage.child();
					newOp->BlockImage = Visit(blockOp, currentSinkingOp);

					newAt = newOp;
				}

				break;
			}

			case OP_TYPE::IM_PATCH:
			{
				Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);

				newOp->base = Visit(newOp->base.child(), currentSinkingOp);
				newOp->patch = Visit(newOp->patch.child(), currentSinkingOp);

				// todo: review if this is always correct, or we need some "divisible" check
				newOp->location[0] = uint16(newOp->location[0] * scaleX);
				newOp->location[1] = uint16(newOp->location[1] * scaleY);

				newAt = newOp;

				break;
			}

			case OP_TYPE::IM_MIPMAP:
			{
				Ptr<ASTOpImageMipmap> newOp = mu::Clone<ASTOpImageMipmap>(at.get());
				Ptr<ASTOp> baseOp = newOp->Source.child();
				newOp->Source = Visit(baseOp, currentSinkingOp);
				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_INTERPOLATE:
			{
				Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(at);

				for (int v = 0; v < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++v)
				{
					Ptr<ASTOp> child = newOp->children[newOp->op.args.ImageInterpolate.targets[v]].child();
					Ptr<ASTOp> bOp = Visit(child, currentSinkingOp);
					newOp->SetChild(newOp->op.args.ImageInterpolate.targets[v], bOp);
				}

				newAt = newOp;
				break;
			}

			case OP_TYPE::IM_MULTILAYER:
			{
				Ptr<ASTOpImageMultiLayer> nop = mu::Clone<ASTOpImageMultiLayer>(at);
				nop->base = Visit(nop->base.child(), currentSinkingOp);
				nop->mask = Visit(nop->mask.child(), currentSinkingOp);
				nop->blend = Visit(nop->blend.child(), currentSinkingOp);
				newAt = nop;
				break;
			}

			case OP_TYPE::IM_LAYER:
			{
				Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(at);

				Ptr<ASTOp> aOp = nop->base.child();
				nop->base = Visit(aOp, currentSinkingOp);

				Ptr<ASTOp> bOp = nop->blend.child();
				nop->blend = Visit(bOp, currentSinkingOp);

				Ptr<ASTOp> mOp = nop->mask.child();
				nop->mask = Visit(mOp, currentSinkingOp);

				newAt = nop;
				break;
			}

			case OP_TYPE::IM_LAYERCOLOUR:
			{
				Ptr<ASTOpImageLayerColor> nop = mu::Clone<ASTOpImageLayerColor>(at);

				Ptr<ASTOp> aOp = nop->base.child();
				nop->base = Visit(aOp, currentSinkingOp);

				Ptr<ASTOp> mOp = nop->mask.child();
				nop->mask = Visit(mOp, currentSinkingOp);

				newAt = nop;
				break;
			}

			case OP_TYPE::IM_DISPLACE:
			{
				Ptr<ASTOpFixed> nop = mu::Clone<ASTOpFixed>(at);

				Ptr<ASTOp> sourceOp = nop->children[nop->op.args.ImageDisplace.source].child();
				nop->SetChild(nop->op.args.ImageDisplace.source, Visit(sourceOp, currentSinkingOp));

				Ptr<ASTOp> mapOp = nop->children[nop->op.args.ImageDisplace.displacementMap].child();
				nop->SetChild(nop->op.args.ImageDisplace.displacementMap, Visit(mapOp, currentSinkingOp));

				// Make sure we don't scale a constant displacement map, which is very wrong.
				if (mapOp->GetOpType() == OP_TYPE::IM_MAKEGROWMAP)
				{
					newAt = nop;
				}
				else
				{
					// We cannot resize an already calculated displacement map.
				}

				break;
			}

			case OP_TYPE::IM_MAKEGROWMAP:
			{
				Ptr<ASTOpImageMakeGrowMap> nop = mu::Clone<ASTOpImageMakeGrowMap>(at);
				Ptr<ASTOp> maskOp = nop->Mask.child();
				nop->Mask = Visit(maskOp, currentSinkingOp);
				newAt = nop;
				break;
			}

			case OP_TYPE::IM_INVERT:
			{
				Ptr<ASTOpFixed> nop = mu::Clone<ASTOpFixed>(at);
				Ptr<ASTOp> maskOp = nop->children[nop->op.args.ImageInvert.base].child();
				nop->SetChild(nop->op.args.ImageInvert.base, Visit(maskOp, currentSinkingOp));
				newAt = nop;
				break;
			}

			case OP_TYPE::IM_SATURATE:
			{
				Ptr<ASTOpFixed> nop = mu::Clone<ASTOpFixed>(at);
				Ptr<ASTOp> maskOp = nop->children[nop->op.args.ImageSaturate.base].child();
				nop->SetChild(nop->op.args.ImageSaturate.base, Visit(maskOp, currentSinkingOp));
				newAt = nop;
				break;
			}

			case OP_TYPE::IM_TRANSFORM:
			{
				// It can only sink in the transform if it doesn't have it's own size.
				const ASTOpImageTransform* TypedAt = static_cast<const ASTOpImageTransform*>(at.get());
				if (TypedAt->SizeX == 0 && TypedAt->SizeY == 0)
				{
					Ptr<ASTOpImageTransform> NewOp = mu::Clone<ASTOpImageTransform>(at);
					Ptr<ASTOp> MaskOp = NewOp->Base.child();
					NewOp->Base = Visit(MaskOp, currentSinkingOp);
					newAt = NewOp;
				}

				break;
			}

			case OP_TYPE::IM_RASTERMESH:
			{
				Ptr<ASTOpImageRasterMesh> nop = mu::Clone<ASTOpImageRasterMesh>(at);
				Ptr<ASTOp> maskOp = nop->mask.child();
				nop->mask = Visit(maskOp, currentSinkingOp);

				// Resize the image to project as well, assuming that since the target has a different resolution
				// it make sense for the source image to have a similar resize.
				// Actually, don't do it because the LODBias will be applied separetely at graph generation time.
				//auto imageOp = nop->image.child();
				//nop->image = Visit(imageOp, currentSinkingOp);

				nop->SizeX = uint16(nop->SizeX * scaleX + 0.5f);
				nop->SizeY = uint16(nop->SizeY * scaleY + 0.5f);
				newAt = nop;
				break;
			}

			default:
				break;
			}

			// end on line, replace with sinking op
			if (at == newAt && at != m_initialSource)
			{
				Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(currentSinkingOp);
				check(newOp->GetOpType() == OP_TYPE::IM_RESIZEREL);

				newOp->SetChild(newOp->op.args.ImageResizeRel.source, at);

				newAt = newOp;
			}

			OldToNew.Add({ at, currentSinkingOp }, newAt);

            return newAt;
        }
    };


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> ASTOpFixed::OptimiseSize() const
    {
        Ptr<ASTOp> at;

        OP_TYPE type = GetOpType();
        switch ( type )
        {

        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_RESIZE:
        {
            auto sourceAt = children[op.args.ImageResize.source].child();

            // The instruction can be sunk
            OP_TYPE sourceType = sourceAt->GetOpType();
            switch ( sourceType )
            {
            case OP_TYPE::IM_RESIZE:
            {
                // Keep top resize
                Ptr<const ASTOpFixed> sourceOp = static_cast<const ASTOpFixed*>(sourceAt.get());

                Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(this);
                newOp->SetChild( newOp->op.args.ImageResize.source, sourceOp->children[sourceOp->op.args.ImageResize.source]);

                at = newOp;
                break;
            }

            case OP_TYPE::IM_PLAINCOLOUR:
            {
                // Set the size in the children and remove resize
				Ptr<ASTOpFixed> sourceOp = mu::Clone<ASTOpFixed>(sourceAt.get());
                sourceOp->op.args.ImagePlainColour.size[0] = op.args.ImageResize.size[0];
				sourceOp->op.args.ImagePlainColour.size[1] = op.args.ImageResize.size[1];
				sourceOp->op.args.ImagePlainColour.LODs = 1; // TODO
				at = sourceOp;
                break;
            }

			case OP_TYPE::IM_TRANSFORM:
			{
				// Set the size in the children and remove resize
				Ptr<ASTOpImageTransform> sourceOp = mu::Clone<ASTOpImageTransform>(sourceAt.get());
				sourceOp->SizeX = op.args.ImageResize.size[0];
				sourceOp->SizeY = op.args.ImageResize.size[1];
				at = sourceOp;
				break;
			}

            case OP_TYPE::IM_CONDITIONAL:
            {
                // We move the resize down the two paths
                auto newOp = mu::Clone<ASTOpConditional>(sourceAt);

                Ptr<ASTOpFixed> aOp = mu::Clone<ASTOpFixed>(this);
                aOp->SetChild( aOp->op.args.ImageResize.source,newOp->yes);
                newOp->yes = aOp;

                Ptr<ASTOpFixed> bOp = mu::Clone<ASTOpFixed>(this);
                bOp->SetChild( bOp->op.args.ImageResize.source,newOp->no);
                newOp->no = bOp;

                at = newOp;
                break;
            }

            case OP_TYPE::IM_SWITCH:
            {
                // Move the resize down all the paths
                Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(sourceAt);

                if (newOp->def)
                {
                    Ptr<ASTOpFixed> defOp = mu::Clone<ASTOpFixed>(this);
                    defOp->SetChild( defOp->op.args.ImageResize.source, newOp->def);
                    newOp->def = defOp;
                }

                for ( auto& cas:newOp->cases )
                {
                    if ( cas.branch )
                    {
                        Ptr<ASTOpFixed> bOp = mu::Clone<ASTOpFixed>(this);
                        bOp->SetChild( bOp->op.args.ImageResize.source, cas.branch);
                        cas.branch = bOp;
                    }
                }

                at = newOp;
                break;
            }

			case OP_TYPE::IM_SWIZZLE:
			{
				Ptr<ASTOpImageSwizzle> newOp = mu::Clone<ASTOpImageSwizzle>(sourceAt);
				for (int s = 0; s < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++s)
				{
					Ptr<ASTOp> OldChannelOp = newOp->Sources[s].child();
					if (OldChannelOp)
					{
						Ptr<ASTOpFixed> ChannelResize = mu::Clone<ASTOpFixed>(this);
						ChannelResize->SetChild(ChannelResize->op.args.ImageResize.source, OldChannelOp);
						newOp->Sources[s] = ChannelResize;
					}
				}
				at = newOp;
				break;
			}

            case OP_TYPE::IM_COMPOSE:
            {
                Ptr<ASTOpImageCompose> newOp = mu::Clone<ASTOpImageCompose>(sourceAt);

                auto aOp = mu::Clone<ASTOpFixed>(this);
                aOp->SetChild( aOp->op.args.ImageResize.source, newOp->Base.child());
                newOp->Base = aOp;

                auto bOp = mu::Clone<ASTOpFixed>(this);
                bOp->SetChild( bOp->op.args.ImageResize.source, newOp->BlockImage.child());
                newOp->BlockImage = bOp;

                if ( newOp->Mask )
                {
                    auto maskOp = mu::Clone<ASTOpFixed>(this);
                    maskOp->SetChild( maskOp->op.args.ImageResize.source, newOp->Mask.child());
                    newOp->Mask = maskOp;
                }

                at = newOp;
                break;
            }

            case OP_TYPE::IM_RASTERMESH:
            {
                Ptr<ASTOpImageRasterMesh> newOp = mu::Clone<ASTOpImageRasterMesh>(sourceAt);

                //if ( newOp->op.args.ImageRasterMesh.sizeX != op.args.ImageResize.size[0]
                //     ||
                //     newOp->op.args.ImageRasterMesh.sizeY != op.args.ImageResize.size[1] )
                {
                    newOp->SizeX = op.args.ImageResize.size[0];
                    newOp->SizeY = op.args.ImageResize.size[1];

					if (newOp->mask)
					{
						Ptr<ASTOpFixed> mop = mu::Clone<ASTOpFixed>(this);
						mop->SetChild(mop->op.args.ImageResize.source, newOp->mask.child());
						newOp->mask = mop;
					}

					// Don't apply absolute resizes to the image to raster: it could even enlarge it.
					// This should only be scaled with relative resizes, which come from LOD biases, etc.
					//if (newOp->op.args.ImageRasterMesh.image)
					//{
					//	Ptr<ASTOpFixed> mop = mu::Clone<ASTOpFixed>(this);
					//	mop->SetChild(mop->op.args.ImageResize.source, newOp->children[newOp->op.args.ImageRasterMesh.image].child());
					//	newOp->SetChild(newOp->op.args.ImageRasterMesh.image, mop);
					//}
				}

                at = newOp;
                break;
            }

            case OP_TYPE::IM_INTERPOLATE:
            {
                Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(sourceAt);

                for ( int s=0; s<MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++s )
                {
                    auto targetAt = newOp->children[ newOp->op.args.ImageInterpolate.targets[s] ].child();
                    if ( targetAt )
                    {
                        Ptr<ASTOpFixed> sourceOp = mu::Clone<ASTOpFixed>(this);
                        sourceOp->SetChild( sourceOp->op.args.ImageResize.source, targetAt);
                        newOp->SetChild( newOp->op.args.ImageInterpolate.targets[s], sourceOp );
                    }
                }

                at = newOp;
                break;
            }

			case OP_TYPE::IM_INVERT:
			{
				Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(sourceAt);
				Ptr<ASTOp> BaseAt = NewOp->children[NewOp->op.args.ImageInvert.base].child();

				Ptr<ASTOpFixed> NewBase = mu::Clone<ASTOpFixed>(this);
				NewBase->SetChild(NewBase->op.args.ImageResize.source, BaseAt);

				NewOp->SetChild(NewOp->op.args.ImageInvert.base, NewBase);

				at = NewOp;
				break;
			}

			case OP_TYPE::IM_PIXELFORMAT:
			{				
				// \todo: only if shrinking?
				
				// Only sink the resize if we know that the pixelformat source image is uncompressed.
				Ptr<ASTOpImagePixelFormat> SourceTyped = static_cast<ASTOpImagePixelFormat*>(sourceAt.get());
				FImageDesc PixelFormatSourceDesc = SourceTyped ->Source->GetImageDesc();
				if (PixelFormatSourceDesc.m_format!=EImageFormat::IF_NONE
					&&
					!mu::IsCompressedFormat(PixelFormatSourceDesc.m_format) )
				{
					Ptr<ASTOpImagePixelFormat> NewOp = mu::Clone<ASTOpImagePixelFormat>(sourceAt);
					Ptr<ASTOp> BaseAt = NewOp->Source.child();

					Ptr<ASTOpFixed> NewBase = mu::Clone<ASTOpFixed>(this);
					NewBase->SetChild(NewBase->op.args.ImageResize.source, BaseAt);

					NewOp->Source = NewBase;

					at = NewOp;
				}
				break;
			}

            case OP_TYPE::IM_LAYER:
            {
                Ptr<ASTOpImageLayer> newOp = mu::Clone<ASTOpImageLayer>(sourceAt);

                Ptr<ASTOpFixed> baseOp = mu::Clone<ASTOpFixed>(this);
                baseOp->SetChild( baseOp->op.args.ImageResize.source, newOp->base );
                newOp->base = baseOp;

                Ptr<ASTOpFixed> blendOp = mu::Clone<ASTOpFixed>(this);
                blendOp->SetChild( blendOp->op.args.ImageResize.source, newOp->blend );
                newOp->blend = blendOp;

                auto maskAt = newOp->mask.child();
                if (maskAt)
                {
                    Ptr<ASTOpFixed> maskOp = mu::Clone<ASTOpFixed>(this);
                    maskOp->SetChild( maskOp->op.args.ImageResize.source, maskAt );
                    newOp->mask = maskOp;
                }

                at = newOp;
                break;
            }

            case OP_TYPE::IM_LAYERCOLOUR:
            {
                Ptr<ASTOpImageLayerColor> newOp = mu::Clone<ASTOpImageLayerColor>(sourceAt);

                Ptr<ASTOpFixed> baseOp = mu::Clone<ASTOpFixed>(this);
                baseOp->SetChild( baseOp->op.args.ImageResize.source, newOp->base );
                newOp->base = baseOp;

                auto maskAt = newOp->mask.child();
                if (maskAt)
                {
                    Ptr<ASTOpFixed> maskOp = mu::Clone<ASTOpFixed>(this);
                    maskOp->SetChild( maskOp->op.args.ImageResize.source, maskAt );
                    newOp->mask = maskOp;
                }

                at = newOp;
                break;
            }

			case OP_TYPE::IM_DISPLACE:
			{
				// In the size optimization phase we can optimize the resize with the displace
				// because the constants have not been collapsed yet.
				// We will still check it and sink the size directly below the IM_MAKEGROWMAP op
				Ptr<ASTOpFixed> SourceTyped = static_cast<ASTOpFixed*>(sourceAt.get());
				Ptr<ASTOp> OriginalDisplacementMapOp = SourceTyped->children[SourceTyped->op.args.ImageDisplace.displacementMap].m_child;
				if (OriginalDisplacementMapOp->GetOpType() == OP_TYPE::IM_MAKEGROWMAP)
				{
					Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(sourceAt);

					Ptr<ASTOpFixed> baseOp = mu::Clone<ASTOpFixed>(this);
					baseOp->SetChild(baseOp->op.args.ImageResize.source,
						newOp->children[newOp->op.args.ImageDisplace.source]);
					newOp->SetChild(newOp->op.args.ImageDisplace.source, baseOp);

					// Clone the map op and replace its children
					Ptr<ASTOpImageMakeGrowMap> mapOp = mu::Clone<ASTOpImageMakeGrowMap>(OriginalDisplacementMapOp);
					newOp->SetChild(newOp->op.args.ImageDisplace.displacementMap, mapOp);

					Ptr<ASTOpFixed> mapSourceOp = mu::Clone<ASTOpFixed>(this);
					mapSourceOp->SetChild(mapSourceOp->op.args.ImageResize.source, mapOp->Mask);
					mapOp->Mask = mapSourceOp;

					at = newOp;
				}
				break;
			}

            default:
                break;
            }

            break;
        }



        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_RESIZEREL:
        {
            auto sourceAt = children[op.args.ImageResizeRel.source].child();

            // The instruction can be sunk
            OP_TYPE sourceType = sourceAt->GetOpType();
            switch ( sourceType )
            {

			case OP_TYPE::IM_BLANKLAYOUT:
			{
				Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(sourceAt);

				newOp->op.args.ImageBlankLayout.blockSize[0] =
					uint16(newOp->op.args.ImageBlankLayout.blockSize[0]
						* op.args.ImageResizeRel.factor[0]
						+ 0.5f);
				newOp->op.args.ImageBlankLayout.blockSize[1] =
					uint16(newOp->op.args.ImageBlankLayout.blockSize[1]
						* op.args.ImageResizeRel.factor[1]
						+ 0.5f);
				at = newOp;
				break;
			}

			case OP_TYPE::IM_PLAINCOLOUR:
			{
				Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(sourceAt);

				newOp->op.args.ImagePlainColour.size[0] = FMath::CeilToInt( newOp->op.args.ImagePlainColour.size[0] * op.args.ImageResizeRel.factor[0] );
				newOp->op.args.ImagePlainColour.size[1] = FMath::CeilToInt( newOp->op.args.ImagePlainColour.size[1] * op.args.ImageResizeRel.factor[1] );
				newOp->op.args.ImagePlainColour.LODs = 1; // TODO
				at = newOp;
				break;
			}

			case OP_TYPE::IM_TRANSFORM:
			{
				// We can only optimize here if we know the transform result size, otherwise, we will sink the op in the sinker.
				const ASTOpImageTransform* typedAt = static_cast<const ASTOpImageTransform*>(sourceAt.get());
				if (typedAt->SizeX != 0 && typedAt->SizeY != 0)
				{
					// Set the size in the children and remove resize
					Ptr<ASTOpImageTransform> sourceOp = mu::Clone<ASTOpImageTransform>(sourceAt.get());
					sourceOp->SizeX = FMath::CeilToInt32(sourceOp->SizeX * op.args.ImageResizeRel.factor[0]);
					sourceOp->SizeY = FMath::CeilToInt32(sourceOp->SizeY * op.args.ImageResizeRel.factor[1]);
					at = sourceOp;
				}
				break;
			}

				// Don't combine. ResizeRel sometimes can resize more children than Resize can do. (see RasterMesh)
				// It can be combined in an optimization step further in the process, when normal sizes may have been 
				// optimized already (see OptimizeSemantic)
//                case OP_TYPE::IM_RESIZE:
//                {
//                    break;
//                }


            default:
            {
                Sink_ImageResizeRelAST sinker;
                at = sinker.Apply(this);

                break;
            }

            }

            break;
        }


        //-------------------------------------------------------------------------------------
        case OP_TYPE::IM_RESIZELIKE:
        {
            Ptr<ImageSizeExpression> sourceSize =
                    children[op.args.ImageResizeLike.source]->GetImageSizeExpression();
            Ptr<ImageSizeExpression> sizeSourceSize =
                    children[op.args.ImageResizeLike.sizeSource]->GetImageSizeExpression();

            if ( *sourceSize == *sizeSourceSize )
            {
                at = children[op.args.ImageResizeLike.source].child();
            }

            else if ( sizeSourceSize->type == ImageSizeExpression::ISET_CONSTANT )
            {
                Ptr<ASTOpFixed> newAt = new ASTOpFixed;
                newAt->op.type = OP_TYPE::IM_RESIZE;
                newAt->SetChild( newAt->op.args.ImageResize.source, children[op.args.ImageResizeLike.source] );
                newAt->op.args.ImageResize.size[0] = sizeSourceSize->size[0];
                newAt->op.args.ImageResize.size[1] = sizeSourceSize->size[1];
                at = newAt;
            }
            else if ( sizeSourceSize->type == ImageSizeExpression::ISET_LAYOUTFACTOR )
            {
                // TODO
                // Skip intermediate ops until the layout
//                    if ( program.m_code[sizeSourceAt].type != OP_TYPE::IM_BLANKLAYOUT )
//                    {
//                        m_modified = true;

//                        OP blackLayoutOp;
//                        blackLayoutOp.type = OP_TYPE::IM_BLANKLAYOUT;
//                        blackLayoutOp.args.ImageBlankLayout.layout = sizeSourceSize->layout;
//                        blackLayoutOp.args.ImageBlankLayout.blockSize[0] = sizeSourceSize->factor[0];
//                        blackLayoutOp.args.ImageBlankLayout.blockSize[1] = sizeSourceSize->factor[1];
//                        blackLayoutOp.args.ImageBlankLayout.format = IF_L_UBYTE;

//                        OP opResize = program.m_code[at];
//                        opResize.args.ImageResizeLike.sizeSource = program.AddOp( blackLayoutOp );
//                        at = program.AddOp( opResize );
//                    }
            }

            break;
        }

        default:
            break;

        }

        return at;
    }

}
