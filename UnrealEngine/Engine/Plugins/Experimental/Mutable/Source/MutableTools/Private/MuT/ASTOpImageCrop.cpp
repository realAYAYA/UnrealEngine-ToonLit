// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageCrop.h"

#include "Containers/Map.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"


namespace mu
{

	ASTOpImageCrop::ASTOpImageCrop()
		: Source(this)
	{
	}


	ASTOpImageCrop::~ASTOpImageCrop()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageCrop::IsEqual(const ASTOp& InOther) const
	{
		if (InOther.GetOpType()==GetOpType())
		{
			const ASTOpImageCrop* other = static_cast<const ASTOpImageCrop*>(&InOther);
			return Source == other->Source &&
				Min == other->Min &&
				Size == other->Size;
		}
		return false;
	}


	uint64 ASTOpImageCrop::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(OP_TYPE::IM_CROP);
		hash_combine(res, Source.child().get());
		hash_combine(res, Min[0]);
		hash_combine(res, Min[1]);
		hash_combine(res, Size[0]);
		hash_combine(res, Size[1]);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImageCrop::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageCrop> n = new ASTOpImageCrop();
		n->Source = mapChild(Source.child());
		n->Min = Min;
		n->Size = Size;
		return n;
	}


	void ASTOpImageCrop::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	void ASTOpImageCrop::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageCropArgs args;
			memset(&args, 0, sizeof(args));

			if (Source) args.source = Source->linkedAddress;
			args.minX = Min[0];
			args.minY = Min[1];
			args.sizeX = Size[0];
			args.sizeY = Size[1];

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::IM_CROP);
			AppendCode(program.m_byteCode, args);
		}

	}


	FImageDesc ASTOpImageCrop::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const
	{
		FImageDesc Result;

		// Local context in case it is necessary
		FGetImageDescContext localContext;
		if (!context)
		{
			context = &localContext;
		}
		else
		{
			// Cached result?
			FImageDesc* PtrValue = context->m_results.Find(this);
			if (PtrValue)
			{
				return *PtrValue;
			}
		}

		// Actual work
		if (Source)
		{
			Result = Source->GetImageDesc(returnBestOption, context);
			//Result.m_lods = 1;
			Result.m_size[0] = Size[0];
			Result.m_size[1] = Size[1];
		}


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, Result);
		}

		return Result;
	}


	mu::Ptr<ImageSizeExpression> ASTOpImageCrop::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
		pRes->type = ImageSizeExpression::ISET_CONSTANT;
		pRes->size[0] = Size[0];
		pRes->size[1] = Size[1];
		return pRes;
	}


	void ASTOpImageCrop::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		// We didn't find any layout yet.
		*pBlockX = 0;
		*pBlockY = 0;

		// Try the source
		if (Source)
		{
			Source->GetLayoutBlockSize( pBlockX, pBlockY );
		}
	}


	Ptr<ASTOp> ASTOpImageCrop::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
	{
		Ptr<ASTOp> at;

		Ptr<ASTOp> sourceAt = Source.child();

		// The instruction can be sunk
		OP_TYPE sourceType = sourceAt->GetOpType();
		switch (sourceType)
		{
		case OP_TYPE::IM_PLAINCOLOUR:
		{
			Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(sourceAt.get());
			NewOp->op.args.ImagePlainColour.size[0] = Size[0];
			NewOp->op.args.ImagePlainColour.size[1] = Size[1];
			NewOp->op.args.ImagePlainColour.LODs = 1; // TODO
			at = NewOp;
			break;
		}

		default:
			break;

		}

		return at;
	}


	Ptr<ASTOp> ASTOpImageCrop::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		Ptr<ASTOp> sourceAt = Source.child();

		switch (sourceAt->GetOpType())
		{
			// In case we have other operations with special optimisation rules.
		case OP_TYPE::NONE:
			break;

		default:
		{
			at = context.ImageCropSinker.Apply(this);

			break;
		} // default

		}


		return at;
	}


	Ptr<ASTOp> Sink_ImageCropAST::Apply(const ASTOpImageCrop* InRoot)
	{
		check(InRoot->GetOpType() == OP_TYPE::IM_CROP);

		m_root = InRoot;
		OldToNew.Reset();

		m_initialSource = InRoot->Source.child();
		Ptr<ASTOp> newSource = Visit(m_initialSource, InRoot);

		// If there is any change, it is the new root.
		if (newSource != m_initialSource)
		{
			return newSource;
		}

		return nullptr;
	}


	Ptr<ASTOp> Sink_ImageCropAST::Visit(Ptr<ASTOp> at, const ASTOpImageCrop* currentCropOp)
	{
		if (!at) return nullptr;

		// Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at, currentCropOp });
		if (Cached)
		{
			return *Cached;
		}

		bool skipSinking = false;
		Ptr<ASTOp> newAt = at;
		switch (at->GetOpType())
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
			for (auto& c : newOp->cases)
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
			const ASTOpImagePatch* typedPatch = static_cast<const ASTOpImagePatch*>(at.get());

			Ptr<ASTOp> rectOp = typedPatch->patch.child();
			ASTOp::FGetImageDescContext context;
			FImageDesc patchDesc = rectOp->GetImageDesc(false, &context);
			box<FIntVector2> patchBox;
			patchBox.min[0] = typedPatch->location[0];
			patchBox.min[1] = typedPatch->location[1];
			patchBox.size[0] = patchDesc.m_size[0];
			patchBox.size[1] = patchDesc.m_size[1];

			box<FIntVector2> cropBox;
			cropBox.min[0] = currentCropOp->Min[0];
			cropBox.min[1] = currentCropOp->Min[1];
			cropBox.size[0] = currentCropOp->Size[0];
			cropBox.size[1] = currentCropOp->Size[1];

			if (!patchBox.IntersectsExclusive(cropBox))
			{
				// We can ignore the patch
				newAt = Visit(typedPatch->base.child(), currentCropOp);
			}
			else
			{
				// Crop the base with the full crop, and the patch with the intersected part,
				// adapting the patch origin
				Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);
				newOp->base = Visit(newOp->base.child(), currentCropOp);

				box<FIntVector2> ibox = patchBox.Intersect2i(cropBox);
				check(ibox.size[0] > 0 && ibox.size[1] > 0);

				Ptr<ASTOpImageCrop> patchCropOp = mu::Clone<ASTOpImageCrop>(currentCropOp);
				patchCropOp->Min[0] = ibox.min[0] - patchBox.min[0];
				patchCropOp->Min[1] = ibox.min[1] - patchBox.min[1];
				patchCropOp->Size[0] = ibox.size[0];
				patchCropOp->Size[1] = ibox.size[1];
				newOp->patch = Visit(newOp->patch.child(), patchCropOp.get());

				newOp->location[0] = ibox.min[0] - cropBox.min[0];
				newOp->location[1] = ibox.min[1] - cropBox.min[1];
				newAt = newOp;
			}

			break;
		}

		case OP_TYPE::IM_CROP:
		{
			// We can combine the two crops into a possibly smaller crop
			const ASTOpImageCrop* childCrop = static_cast<const ASTOpImageCrop*>(at.get());

			box<FIntVector2> childCropBox;
			childCropBox.min[0] = childCrop->Min[0];
			childCropBox.min[1] = childCrop->Min[1];
			childCropBox.size[0] = childCrop->Size[0];
			childCropBox.size[1] = childCrop->Size[1];

			box<FIntVector2> cropBox;
			cropBox.min[0] = currentCropOp->Min[0];
			cropBox.min[1] = currentCropOp->Min[1];
			cropBox.size[0] = currentCropOp->Size[0];
			cropBox.size[1] = currentCropOp->Size[1];

			// Compose the crops: in the final image the child crop is applied first and the
			// current ctop is applied to the result. So the final crop box would be:
			box<FIntVector2> ibox;
			ibox.min = childCropBox.min + cropBox.min;
			ibox.size[0] = FMath::Min(cropBox.size[0], childCropBox.size[0]);
			ibox.size[1] = FMath::Min(cropBox.size[1], childCropBox.size[1]);
			//check(cropBox.min.AllSmallerOrEqualThan(childCropBox.size));
			//check((cropBox.min + cropBox.size).AllSmallerOrEqualThan(childCropBox.size));
			//check((ibox.min + ibox.size).AllSmallerOrEqualThan(childCropBox.min + childCropBox.size));

			// This happens more often that one would think
			if (ibox == childCropBox)
			{
				// the parent crop is not necessary
				skipSinking = true;
			}
			else if (ibox == cropBox)
			{
				// The child crop is not necessary
				Ptr<ASTOp> childSource = childCrop->Source.child();
				newAt = Visit(childSource, currentCropOp);
			}
			else
			{
				// combine into one crop
				Ptr<ASTOpImageCrop> newCropOp = mu::Clone<ASTOpImageCrop>(currentCropOp);
				newCropOp->Min[0] = ibox.min[0];
				newCropOp->Min[1] = ibox.min[1];
				newCropOp->Size[0] = ibox.size[0];
				newCropOp->Size[1] = ibox.size[1];

				Ptr<ASTOp> childSource = childCrop->Source.child();
				newAt = Visit(childSource, newCropOp.get());
			}
			break;
		}

		case OP_TYPE::IM_LAYER:
		{
			// We move the op down the arguments
			Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(at);

			Ptr<ASTOp> aOp = nop->base.child();
			nop->base = Visit(aOp, currentCropOp);

			Ptr<ASTOp> bOp = nop->blend.child();
			nop->blend = Visit(bOp, currentCropOp);

			Ptr<ASTOp> mOp = nop->mask.child();
			nop->mask = Visit(mOp, currentCropOp);

			newAt = nop;
			break;
		}

		case OP_TYPE::IM_LAYERCOLOUR:
		{
			// We move the op down the arguments
			Ptr<ASTOpImageLayerColor> nop = mu::Clone<ASTOpImageLayerColor>(at);

			Ptr<ASTOp> aOp = nop->base.child();
			nop->base = Visit(aOp, currentCropOp);

			Ptr<ASTOp> mOp = nop->mask.child();
			nop->mask = Visit(mOp, currentCropOp);

			newAt = nop;
			break;
		}

		case OP_TYPE::IM_DISPLACE:
		{
			// We move the op down the arguments
			Ptr<ASTOpFixed> nop = mu::Clone<ASTOpFixed>(at);

			Ptr<ASTOp> aOp = nop->children[nop->op.args.ImageDisplace.source].child();
			nop->SetChild(nop->op.args.ImageDisplace.source, Visit(aOp, currentCropOp));

			Ptr<ASTOp> bOp = nop->children[nop->op.args.ImageDisplace.displacementMap].child();
			nop->SetChild(nop->op.args.ImageDisplace.displacementMap, Visit(bOp, currentCropOp));

			newAt = nop;
			break;
		}

		case OP_TYPE::IM_RASTERMESH:
		{
			// We add cropping data to the raster mesh if it doesn't have any
			// \TODO: Is is possible to hit 2 crops on a raster mesh? Combine the crop.
			Ptr<ASTOpImageRasterMesh> nop = mu::Clone<ASTOpImageRasterMesh>(at);

			bool bRasterHasCrop = nop->UncroppedSizeX != 0;
			if (!bRasterHasCrop)
			{
				box<FIntVector2> cropBox;
				cropBox.min[0] = currentCropOp->Min[0];
				cropBox.min[1] = currentCropOp->Min[1];
				cropBox.size[0] = currentCropOp->Size[0];
				cropBox.size[1] = currentCropOp->Size[1];

				nop->UncroppedSizeX = nop->SizeX;
				nop->UncroppedSizeY = nop->SizeY;
				nop->CropMinX = cropBox.min[0];
				nop->CropMinY = cropBox.min[1];
				nop->SizeX = cropBox.size[0];
				nop->SizeY = cropBox.size[1];

				newAt = nop;
			}
			break;
		}

		case OP_TYPE::IM_INTERPOLATE:
		{
			// Move the op  down all the paths
			auto newOp = mu::Clone<ASTOpFixed>(at);

			for (int v = 0; v < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++v)
			{
				Ptr<ASTOp> child = newOp->children[newOp->op.args.ImageInterpolate.targets[v]].child();
				Ptr<ASTOp> bOp = Visit(child, currentCropOp);
				newOp->SetChild(newOp->op.args.ImageInterpolate.targets[v], bOp);
			}

			newAt = newOp;
			break;
		}

		default:
			break;
		}

		// end on line, replace with crop
		if (at == newAt && at != m_initialSource && !skipSinking)
		{
			Ptr<ASTOpImageCrop> newOp = mu::Clone<ASTOpImageCrop>(currentCropOp);
			check(newOp->GetOpType() == OP_TYPE::IM_CROP);

			newOp->Source = at;

			newAt = newOp;
		}

		OldToNew.Add({ at, currentCropOp }, newAt);

		return newAt;
	}


}
