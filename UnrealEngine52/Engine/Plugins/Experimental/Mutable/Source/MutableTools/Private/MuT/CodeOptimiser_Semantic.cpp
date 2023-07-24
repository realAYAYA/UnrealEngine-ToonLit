// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/ImagePrivate.h"
#include "MuR/ImageRLE.h"
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
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/Table.h"

#include <memory>
#include <utility>

namespace mu
{


    //---------------------------------------------------------------------------------------------
    //! Get an uncompressed version of the image.
    //! If the image is not compressed, it si returned unmodified.
    //---------------------------------------------------------------------------------------------
    ImagePtrConst GetUncompressed( ImagePtrConst pSource )
    {
        ImagePtrConst pResult = pSource;

        if ( pSource->GetFormat()== EImageFormat::IF_L_UBYTE_RLE )
        {
            ImagePtr pNew = new Image( pSource->GetSizeX(), pSource->GetSizeY(),
                                 pSource->GetLODCount(),
				EImageFormat::IF_L_UBYTE );
            UncompressRLE_L( pSource.get(), pNew.get() );
            pResult = pNew;
        }
        else if ( pSource->GetFormat()== EImageFormat::IF_L_UBIT_RLE )
        {
            ImagePtr pNew = new Image( pSource->GetSizeX(), pSource->GetSizeY(),
                                 pSource->GetLODCount(),
				EImageFormat::IF_L_UBYTE );
            UncompressRLE_L1( pSource.get(), pNew.get() );
            pResult = pNew;
        }

        return pResult;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    bool SemanticOptimiserAST(
            ASTOpList& roots,
            const FModelOptimizationOptions& optimisationOptions
            )
    {
        MUTABLE_CPUPROFILER_SCOPE(SemanticOptimiserAST);

        bool modified = false;

        // TODO: isn't top down better suited?
        ASTOp::Traverse_BottomUp_Unique( roots, [&](Ptr<ASTOp>& n)
        {
            auto o = n->OptimiseSemantic(optimisationOptions);

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
    Ptr<ASTOp> ASTOpFixed::OptimiseSemantic( const FModelOptimizationOptions& options ) const
    {
        Ptr<ASTOp> at;

        OP_TYPE type = GetOpType();
        switch ( type )
        {

        case OP_TYPE::BO_NOT:
        {
            auto sourceAt = children[op.args.BoolNot.source].child();
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
            auto aAt = children[op.args.BoolBinary.a].child();
            auto bAt = children[op.args.BoolBinary.b].child();
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
                if ( dynamic_cast<const ASTOpConstantBool*>(aAt.get())->value )
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
                if ( dynamic_cast<const ASTOpConstantBool*>(bAt.get())->value )
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
                auto typedA = dynamic_cast<const ASTOpFixed*>(aAt.get());
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
                auto typedB = dynamic_cast<const ASTOpFixed*>(bAt.get());
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
            auto aAt = children[op.args.BoolBinary.a].child();
            auto bAt = children[op.args.BoolBinary.b].child();
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
                if ( dynamic_cast<const ASTOpConstantBool*>(aAt.get())->value )
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
                if ( dynamic_cast<const ASTOpConstantBool*>(bAt.get())->value )
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
                auto typedA = dynamic_cast<const ASTOpFixed*>(aAt.get());
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
                auto typedB = dynamic_cast<const ASTOpFixed*>(bAt.get());
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

		//-------------------------------------------------------------------------------------
		case OP_TYPE::IM_RESIZEREL:
		{
			auto sourceAt = children[op.args.ImageResizeRel.source].child();

			// The instruction can be sunk
			OP_TYPE sourceType = sourceAt->GetOpType();
			switch (sourceType)
			{

			// This is done here instead of in the OptimizeSize step to prevent removing 
			// resize_rel too early.
            case OP_TYPE::IM_RESIZE:
            {
				Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(sourceAt.get());
				NewOp->op.args.ImageResize.size[0] =
                        int16_t(NewOp->op.args.ImageResize.size[0] * op.args.ImageResizeRel.factor[0] );
				NewOp->op.args.ImageResize.size[1] =
                        int16_t(NewOp->op.args.ImageResize.size[1] * op.args.ImageResizeRel.factor[1] );

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
            ASTOpList& roots,
            const FModelOptimizationOptions& optimisationOptions
    )
    {
        MUTABLE_CPUPROFILER_SCOPE(SinkOptimiserAST);

        bool modified = false;

		FOptimizeSinkContext context;

        ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](Ptr<ASTOp>& n)
        {
            auto o = n->OptimiseSink(optimisationOptions, context);
            if (o && n!=o)
            {
                modified = true;
                ASTOp::Replace(n,o);
            }

            return true;
        });

        return modified;
    }


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    Ptr<ASTOp> Sink_ImageCropAST::Apply( const ASTOp* root )
    {
        m_root = root;
        OldToNew.Empty();

        check(root->GetOpType()==OP_TYPE::IM_CROP);

        auto typedRoot = dynamic_cast<const ASTOpFixed*>(root);
        m_initialSource = typedRoot->children[typedRoot->op.args.ImageCrop.source].child();
        Ptr<ASTOp> newSource = Visit( m_initialSource, typedRoot );

        // If there is any change, it is the new root.
        if (newSource!=m_initialSource)
        {
            return newSource;
        }

        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> Sink_ImageCropAST::Visit( Ptr<ASTOp> at, const ASTOpFixed* currentCropOp )
    {
        if (!at) return nullptr;

        // Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at, currentCropOp });
		if (Cached)
		{
			return *Cached;
		}

        bool skipSinking=false;
        Ptr<ASTOp> newAt = at;
        switch ( at->GetOpType() )
        {

        case OP_TYPE::IM_CONDITIONAL:
        {
            // We move down the two paths
            Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
            newOp->yes = Visit(newOp->yes.child(), currentCropOp);
            newOp->no = Visit(newOp->no.child(), currentCropOp);
            newAt = newOp;
            break;
        }

        case OP_TYPE::IM_SWITCH:
        {
            // We move down all the paths
            Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
            newOp->def = Visit(newOp->def.child(), currentCropOp);
            for( auto& c:newOp->cases )
            {
                c.branch = Visit(c.branch.child(), currentCropOp);
            }
            newAt = newOp;
            break;
        }

        case OP_TYPE::IM_PIXELFORMAT:
        {
            Ptr<ASTOpImagePixelFormat> nop = mu::Clone<ASTOpImagePixelFormat>(at);
            nop->Source = Visit(nop->Source.child(), currentCropOp);
            newAt = nop;
            break;
        }

        case OP_TYPE::IM_PATCH:
        {
            const ASTOpImagePatch* typedPatch = dynamic_cast<const ASTOpImagePatch*>(at.get());

            auto rectOp = typedPatch->patch.child();
            ASTOp::FGetImageDescContext context;
            auto patchDesc = rectOp->GetImageDesc( false, &context );
            box<vec2<int16_t>> patchBox;
            patchBox.min = typedPatch->location;
            patchBox.size = patchDesc.m_size;

            box<vec2<int16_t>> cropBox;
            cropBox.min[0] = currentCropOp->op.args.ImageCrop.minX;
            cropBox.min[1] = currentCropOp->op.args.ImageCrop.minY;
            cropBox.size[0] = currentCropOp->op.args.ImageCrop.sizeX;
            cropBox.size[1] = currentCropOp->op.args.ImageCrop.sizeY;

            if (!patchBox.IntersectsExclusive(cropBox))
            {
                // We can ignore the patch
                newAt = Visit( typedPatch->base.child(), currentCropOp );
            }
            else
            {
                // Crop the base with the full crop, and the patch with the intersected part,
                // adapting the patch origin
                auto newOp = mu::Clone<ASTOpImagePatch>(at);
                newOp->base = Visit(newOp->base.child(), currentCropOp);

                box<vec2<int16_t>> ibox = patchBox.Intersect(cropBox);
                check(ibox.size[0]>0 && ibox.size[1]>0);

                Ptr<ASTOpFixed> patchCropOp = mu::Clone<ASTOpFixed>(currentCropOp);
                patchCropOp->op.args.ImageCrop.minX = ibox.min[0] - patchBox.min[0];
                patchCropOp->op.args.ImageCrop.minY = ibox.min[1] - patchBox.min[1];
                patchCropOp->op.args.ImageCrop.sizeX = ibox.size[0];
                patchCropOp->op.args.ImageCrop.sizeY = ibox.size[1];
                newOp->patch = Visit(newOp->patch.child(), patchCropOp.get());

                newOp->location[0] = ibox.min[0] - cropBox.min[0];
                newOp->location[1] = ibox.min[1] - cropBox.min[1];
                newAt = newOp;
            }

            break;
        }

        case OP_TYPE::IM_CROP:
        {
            // We can combine the two crops into a possibliy smaller crop
            auto childCrop = dynamic_cast<const ASTOpFixed*>(at.get());

            box<vec2<int16_t>> childCropBox;
            childCropBox.min[0] = childCrop->op.args.ImageCrop.minX;
            childCropBox.min[1] = childCrop->op.args.ImageCrop.minY;
            childCropBox.size[0] = childCrop->op.args.ImageCrop.sizeX;
            childCropBox.size[1] = childCrop->op.args.ImageCrop.sizeY;

            box<vec2<int16_t>> cropBox;
            cropBox.min[0] = currentCropOp->op.args.ImageCrop.minX;
            cropBox.min[1] = currentCropOp->op.args.ImageCrop.minY;
            cropBox.size[0] = currentCropOp->op.args.ImageCrop.sizeX;
            cropBox.size[1] = currentCropOp->op.args.ImageCrop.sizeY;

            // Compose the crops: in the final image the child crop is applied first and the
            // current ctop is applied to the result. So the final crop box would be:
            box<vec2<int16_t>> ibox;
            ibox.min = childCropBox.min + cropBox.min;
            ibox.size = vec2<int16_t>::min(cropBox.size, childCropBox.size);
            check(cropBox.min.AllSmallerOrEqualThan(childCropBox.size));
            check((cropBox.min + cropBox.size)
                                .AllSmallerOrEqualThan(childCropBox.size));
            check((ibox.min + ibox.size)
                                .AllSmallerOrEqualThan(childCropBox.min + childCropBox.size));

            // This happens more often that one would think
            if (ibox==childCropBox)
            {
                // the parent crop is not necessary
                skipSinking = true;
            }
            else if (ibox == cropBox)
            {
                // The child crop is not necessary
                auto childSource =
                    childCrop->children[childCrop->op.args.ImageCrop.source].child();
                newAt = Visit(childSource, currentCropOp);
            }
            else
            {
                // combine into one crop
                Ptr<ASTOpFixed> newCropOp = mu::Clone<ASTOpFixed>(currentCropOp);
                newCropOp->op.args.ImageCrop.minX = ibox.min[0];
                newCropOp->op.args.ImageCrop.minY = ibox.min[1];
                newCropOp->op.args.ImageCrop.sizeX = ibox.size[0];
                newCropOp->op.args.ImageCrop.sizeY = ibox.size[1];

                auto childSource =
                    childCrop->children[childCrop->op.args.ImageCrop.source].child();
                newAt = Visit(childSource, newCropOp.get());
            }
            break;
        }

        case OP_TYPE::IM_LAYER:
        {
            // We move the op down the arguments
            auto nop = mu::Clone<ASTOpImageLayer>(at);

            auto aOp = nop->base.child();
            nop->base = Visit(aOp, currentCropOp);

            auto bOp = nop->blend.child();
            nop->blend = Visit(bOp, currentCropOp);

            auto mOp = nop->mask.child();
            nop->mask = Visit(mOp, currentCropOp);

            newAt = nop;
            break;
        }

        case OP_TYPE::IM_LAYERCOLOUR:
        {
            // We move the op down the arguments
            auto nop = mu::Clone<ASTOpImageLayerColor>(at);

            auto aOp = nop->base.child();
            nop->base = Visit(aOp, currentCropOp);

            auto mOp = nop->mask.child();
            nop->mask = Visit(mOp, currentCropOp);

            newAt = nop;
            break;
        }

        case OP_TYPE::IM_INTERPOLATE:
        {
            // Move the op  down all the paths
            auto newOp = mu::Clone<ASTOpFixed>(at);

            for (int v = 0; v < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++v)
            {
                auto child =
                    newOp->children[newOp->op.args.ImageInterpolate.targets[v]].child();
                auto bOp = Visit(child, currentCropOp);
                newOp->SetChild(newOp->op.args.ImageInterpolate.targets[v], bOp);
            }

            newAt = newOp;
            break;
        }


        case OP_TYPE::IM_INTERPOLATE3:
        {
            // We move the op down all the paths
            auto newOp = mu::Clone<ASTOpFixed>(at);

            auto top0 = newOp->children[newOp->op.args.ImageInterpolate3.target0].child();
            newOp->SetChild(newOp->op.args.ImageInterpolate3.target0,
                            Visit(top0, currentCropOp));

            auto top1 = newOp->children[newOp->op.args.ImageInterpolate3.target1].child();
            newOp->SetChild(newOp->op.args.ImageInterpolate3.target1,
                            Visit(top1, currentCropOp));

            auto top2 = newOp->children[newOp->op.args.ImageInterpolate3.target2].child();
            newOp->SetChild(newOp->op.args.ImageInterpolate3.target2,
                            Visit(top2, currentCropOp));

            newAt = newOp;
            break;
        }

        default:
            break;
        }

        // end on line, replace with crop
        if (at==newAt && at!=m_initialSource && !skipSinking)
        {
            Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(currentCropOp);
            check(newOp->GetOpType()==OP_TYPE::IM_CROP);

            newOp->SetChild( newOp->op.args.ImageCrop.source,at);

            newAt = newOp;
        }

		OldToNew.Add({ at, currentCropOp }, newAt);

        return newAt;
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
        case OP_TYPE::IM_CROP:
        {
            auto sourceAt = children[op.args.ImageCrop.source].child();

            switch ( sourceAt->GetOpType() )
            {
            // In case we have other operations with special optimisation rules.
            case OP_TYPE::NONE:
                break;

            default:
            {
                at = context.ImageCropSinker.Apply(this);

                break;
            } // default

            } // switch

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
                    auto typedSource = dynamic_cast<const ASTOpConditional*>(sourceAt.get());
                    auto typedDisplacementMap = dynamic_cast<const ASTOpConditional*>(displaceMapAt.get());

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
                    auto typedSource = dynamic_cast<const ASTOpSwitch*>(sourceAt.get());
                    auto typedDisplacementMap = dynamic_cast<const ASTOpSwitch*>(displaceMapAt.get());

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

            auto typedRoot = dynamic_cast<const ASTOpFixed*>(root);
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
				// We move the mask creation down format.
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
					auto typedAt = dynamic_cast<const ASTOpImageCompose*>(at.get());
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
					auto newOp = mu::Clone<ASTOpImageCompose>(at);

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
				auto newOp = mu::Clone<ASTOpImagePatch>(at);

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
				auto newOp = mu::Clone<ASTOpImageMipmap>(at.get());
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


			case OP_TYPE::IM_INTERPOLATE3:
			{
				Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(at);

				Ptr<ASTOp> top0 = newOp->children[newOp->op.args.ImageInterpolate3.target0].child();
				newOp->SetChild(newOp->op.args.ImageInterpolate3.target0, Visit(top0, currentSinkingOp));

				Ptr<ASTOp> top1 = newOp->children[newOp->op.args.ImageInterpolate3.target1].child();
				newOp->SetChild(newOp->op.args.ImageInterpolate3.target1, Visit(top1, currentSinkingOp));

				Ptr<ASTOp> top2 = newOp->children[newOp->op.args.ImageInterpolate3.target2].child();
				newOp->SetChild(newOp->op.args.ImageInterpolate3.target2, Visit(top2, currentSinkingOp));

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
				Ptr<ASTOpImageTransform> nop = mu::Clone<ASTOpImageTransform>(at);
				Ptr<ASTOp> maskOp = nop->base.child();
				nop->base = Visit(maskOp, currentSinkingOp);
				newAt = nop;
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

				nop->sizeX = uint16(nop->sizeX * scaleX + 0.5f);
				nop->sizeY = uint16(nop->sizeY * scaleY + 0.5f);
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
                auto sourceOp = dynamic_cast<const ASTOpFixed*>(sourceAt.get());

                Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(this);
                newOp->SetChild( newOp->op.args.ImageResize.source, sourceOp->children[sourceOp->op.args.ImageResize.source]);

                at = newOp;
                break;
            }

            case OP_TYPE::IM_PLAINCOLOUR:
            {
                // Set the size in the children and remove resize
                auto sourceOp = mu::Clone<ASTOpFixed>(sourceAt.get());
                sourceOp->op.args.ImagePlainColour.size[0] = op.args.ImageResize.size[0];
                sourceOp->op.args.ImagePlainColour.size[1] = op.args.ImageResize.size[1];
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
                    newOp->sizeX = op.args.ImageResize.size[0];
                    newOp->sizeY = op.args.ImageResize.size[1];

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

            case OP_TYPE::IM_INTERPOLATE3:
            {
                Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(sourceAt);

                {
                    Ptr<ASTOpFixed> sourceOp = mu::Clone<ASTOpFixed>(this);
                    auto targetAt = newOp->children[ newOp->op.args.ImageInterpolate3.target0 ].child();
                    sourceOp->SetChild( sourceOp->op.args.ImageResize.source, targetAt);
                    newOp->SetChild( newOp->op.args.ImageInterpolate3.target0, sourceOp );
                }

                {
                    Ptr<ASTOpFixed> sourceOp = mu::Clone<ASTOpFixed>(this);
                    auto targetAt = newOp->children[ newOp->op.args.ImageInterpolate3.target1 ].child();
                    sourceOp->SetChild( sourceOp->op.args.ImageResize.source, targetAt);
                    newOp->SetChild( newOp->op.args.ImageInterpolate3.target1, sourceOp );
                }

                {
                    Ptr<ASTOpFixed> sourceOp = mu::Clone<ASTOpFixed>(this);
                    auto targetAt = newOp->children[ newOp->op.args.ImageInterpolate3.target2 ].child();
                    sourceOp->SetChild( sourceOp->op.args.ImageResize.source, targetAt);
                    newOp->SetChild( newOp->op.args.ImageInterpolate3.target2, sourceOp );
                }

                at = newOp;
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
				Ptr<ASTOpFixed> SourceTyped = dynamic_cast<ASTOpFixed*>(sourceAt.get());
				check(SourceTyped);
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
				auto newOp = mu::Clone<ASTOpFixed>(sourceAt);

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
				auto newOp = mu::Clone<ASTOpFixed>(sourceAt);

				newOp->op.args.ImagePlainColour.size[0] =
					uint16(newOp->op.args.ImagePlainColour.size[0]
						* op.args.ImageResizeRel.factor[0]
						+ 0.5f);
				newOp->op.args.ImagePlainColour.size[1] =
					uint16(newOp->op.args.ImagePlainColour.size[1]
						* op.args.ImageResizeRel.factor[1]
						+ 0.5f);
				at = newOp;
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

