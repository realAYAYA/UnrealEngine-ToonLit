// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageMipmap.h"

#include "Containers/Map.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{

ASTOpImageMipmap::ASTOpImageMipmap()
    : Source(this)
{
}


ASTOpImageMipmap::~ASTOpImageMipmap()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


bool ASTOpImageMipmap::IsEqual(const ASTOp& otherUntyped) const
{
    if (const ASTOpImageMipmap* other = dynamic_cast<const ASTOpImageMipmap*>(&otherUntyped) )
    {
        return Source==other->Source && 
			Levels == other->Levels &&
			BlockLevels == other->BlockLevels &&
			bOnlyTail == other->bOnlyTail &&
			SharpenFactor == other->SharpenFactor &&
			AddressMode == other->AddressMode &&
			FilterType == other->FilterType &&
			DitherMipmapAlpha == other->DitherMipmapAlpha;
	}
    return false;
}


uint64 ASTOpImageMipmap::Hash() const
{
	uint64 res = std::hash<void*>()(Source.child().get() );
    hash_combine( res, Levels);
    return res;
}


mu::Ptr<ASTOp> ASTOpImageMipmap::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpImageMipmap> n = new ASTOpImageMipmap();
    n->Source = mapChild(Source.child());
	n->Levels = Levels;
	n->BlockLevels = BlockLevels;
	n->bOnlyTail = bOnlyTail;
	n->SharpenFactor = SharpenFactor;
	n->AddressMode = AddressMode;
	n->FilterType = FilterType;
	n->DitherMipmapAlpha = DitherMipmapAlpha;

    return n;
}


void ASTOpImageMipmap::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
    f( Source );
}


void ASTOpImageMipmap::Link( PROGRAM& program, const FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::ImageMipmapArgs args;
        FMemory::Memzero( &args, sizeof(args) );

		args.levels = Levels;
		args.blockLevels = BlockLevels;
		args.onlyTail = bOnlyTail;
		args.sharpenFactor = SharpenFactor;
		args.addressMode = AddressMode;
		args.filterType = FilterType;
		args.ditherMipmapAlpha = DitherMipmapAlpha;
		if (Source) args.source = Source->linkedAddress;

        linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
        //program.m_code.push_back(op);
        program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
        AppendCode(program.m_byteCode,OP_TYPE::IM_MIPMAP);
        AppendCode(program.m_byteCode,args);
    }

}


mu::Ptr<ASTOp> ASTOpImageMipmap::OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS& options, OPTIMIZE_SINK_CONTEXT& context) const
{
	mu::Ptr<ASTOp> at;

	mu::Ptr<ASTOp> sourceAt = Source.child();
	switch (sourceAt->GetOpType())
	{

	case OP_TYPE::IM_BLANKLAYOUT:
	{
		// Set the mipmap generation on the blank layout operation
		auto mop = mu::Clone<ASTOpFixed>(sourceAt);
		mop->op.args.ImageBlankLayout.generateMipmaps = 1;    // true
		mop->op.args.ImageBlankLayout.mipmapCount = Levels;
		at = mop;
		break;
	}

	case OP_TYPE::IM_PIXELFORMAT:
	{
		// Swap unless the mipmap operation builds only the tail or is compressed.
		// Otherwise, we could fall in a loop of swapping mipmaps and pixelformats.
		mu::Ptr<ASTOpImageMipmap> mop = mu::Clone<ASTOpImageMipmap>(this);
		mu::Ptr<ASTOpImagePixelFormat> fop = mu::Clone<ASTOpImagePixelFormat>(sourceAt);
		bool isCompressedFormat = IsCompressedFormat(fop->Format);
		if (isCompressedFormat && !mop->bOnlyTail)
		{
			mop->Source = fop->Source.child();
			fop->Source = mop;

			at = fop;
		}
		break;
	}

	default:
	{
		at = context.ImageMipmapSinker.Apply(this);

		break;
	} // mipmap source default

	} // mipmap source type switch

	
	return at;
}


//!
FImageDesc ASTOpImageMipmap::GetImageDesc(bool returnBestOption, GetImageDescContext* context)
{
	FImageDesc res;

	// Local context in case it is necessary
	GetImageDescContext localContext;
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

	if (Source.child())
	{
		res = Source.child()->GetImageDesc( returnBestOption, context);
	}

	int mipLevels = Levels;

	if (mipLevels == 0)
	{
		mipLevels = FMath::Max(
			(int)ceilf(logf((float)res.m_size[0]) / logf(2.0f)),
			(int)ceilf(logf((float)res.m_size[1]) / logf(2.0f))
		);
	}

	res.m_lods = FMath::Max(res.m_lods, (uint8)mipLevels);


	// Cache the result
	if (context)
	{
		context->m_results.Add(this, res);
	}

	return res;
}


void ASTOpImageMipmap::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
{
	if (Source.child())
	{
		// Assume the block size of the biggest mip
		Source.child()->GetLayoutBlockSize(pBlockX, pBlockY);
	}
}


bool ASTOpImageMipmap::IsImagePlainConstant(vec4<float>& colour) const
{
	bool res = false;
	if (Source.child())
	{
		Source.child()->IsImagePlainConstant(colour);
	}
	return res;
}


mu::Ptr<ImageSizeExpression> ASTOpImageMipmap::GetImageSizeExpression() const
{
	mu::Ptr<ImageSizeExpression> pRes;

	if (Source.child())
	{
		pRes = Source.child()->GetImageSizeExpression();
	}
	else
	{
		pRes = new ImageSizeExpression;
	}

	return pRes;
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> Sink_ImageMipmapAST::Apply(const ASTOp* root)
{
	check(root->GetOpType() == OP_TYPE::IM_MIPMAP);

	m_root = dynamic_cast<const ASTOpImageMipmap*>(root);
	m_oldToNew.Empty();
	m_newOps.Empty();

	if (m_root->bOnlyTail)
	{
		return nullptr;
	}

	mu::Ptr<ASTOp> newSource;
	m_initialSource = m_root->Source.child();

	// Before sinking, see if it is worth splitting into miptail and mip.
	if (!m_root->bOnlyTail
		// \TODO: Review this: It seems it fails for a BC1 8x4 texture having blockSize=4, and level=0
			//&&
			//m_root->Levels
			//!=
			//m_root->BlockLevels 
		)
	{
		// the block mipmaps can be done before composition is done.
		mu::Ptr<ASTOpImageMipmap> newMip = mu::Clone<ASTOpImageMipmap>(m_root);
		newMip->Levels = m_root->BlockLevels;
		newMip->BlockLevels = m_root->BlockLevels;
		newMip->bOnlyTail = false;

		// the smallest mipmaps after the composition is done.
		mu::Ptr<ASTOpImageMipmap> topMipOp = mu::Clone<ASTOpImageMipmap>(m_root);
		topMipOp->bOnlyTail = true;

		// Proceed
		topMipOp->Source = Visit(m_initialSource, newMip.get());

		newSource = topMipOp;
	}
	else
	{
		// Proceed
		newSource = Visit(m_initialSource, m_root);
	}

	m_root = nullptr;

	// If there is any change, it is the new root.
	if (newSource != m_initialSource)
	{
		return newSource;
	}

	return nullptr;
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> Sink_ImageMipmapAST::Visit(mu::Ptr<ASTOp> at, const ASTOpImageMipmap* currentMipmapOp)
{
	if (!at) return nullptr;

	// Newly created?
	if (m_newOps.Contains(at))
	{
		return at;
	}

	// Already visited?
	auto cacheIt = m_oldToNew.Find(at);
	if (cacheIt)
	{
		return *cacheIt;
	}

	mu::Ptr<ASTOp> newAt = at;
	switch (at->GetOpType())
	{

	case OP_TYPE::IM_CONDITIONAL:
	{
		// We move the mask creation down the two paths
		auto newOp = mu::Clone<ASTOpConditional>(at);
		newOp->yes = Visit(newOp->yes.child(), currentMipmapOp);
		newOp->no = Visit(newOp->no.child(), currentMipmapOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::IM_SWITCH:
	{
		// We move the mask creation down all the paths
		auto newOp = mu::Clone<ASTOpSwitch>(at);
		newOp->def = Visit(newOp->def.child(), currentMipmapOp);
		for (auto& c : newOp->cases)
		{
			c.branch = Visit(c.branch.child(), currentMipmapOp);
		}
		newAt = newOp;
		break;
	}

	case OP_TYPE::IM_COMPOSE:
	{
		const ASTOpImageCompose* typedAt = dynamic_cast<const ASTOpImageCompose*>(at.get());
		if (!currentMipmapOp->bOnlyTail
			&&
			// Don't move the mipmapping if we are composing with a mask.
			// TODO: allow mipmapping in the masks, RLE formats, etc.
			!typedAt->Mask
			)
		{
			auto newOp = mu::Clone<ASTOpImageCompose>(at);

			auto baseOp = newOp->Base.child();
			newOp->Base = Visit(baseOp, currentMipmapOp);

			auto blockOp = newOp->BlockImage.child();
			newOp->BlockImage = Visit(blockOp, currentMipmapOp);

			newAt = newOp;
		}

		break;
	}

	case OP_TYPE::IM_PATCH:
	{
		if (!currentMipmapOp->bOnlyTail)
		{
			// Special case: we propagate the mipmapping down the patch up to
			// the level allowed by the patch size and placement. We then leave
			// a top-level  mipmapping operation to generate the smallest
			// mipmaps after the patch is done.

			const ASTOpImagePatch* typedSource = dynamic_cast<const ASTOpImagePatch*>(at.get());
			auto rectOp = typedSource->patch.child();
			ASTOp::GetImageDescContext context;
			auto patchDesc = rectOp->GetImageDesc(false, &context);

			uint8_t blockLevels = 0;
			{
				uint16 minX = typedSource->location[0];
				uint16 minY = typedSource->location[1];
				uint16 sizeX = patchDesc.m_size[0];
				uint16 sizeY = patchDesc.m_size[1];
				while (minX && minY && sizeX && sizeY
					&&
					minX % 2 == 0 && minY % 2 == 0 && sizeX % 2 == 0 && sizeY % 2 == 0)
				{
					blockLevels++;
					minX /= 2;
					minY /= 2;
					sizeX /= 2;
					sizeY /= 2;
				}
			}

			mu::Ptr<ASTOpImageMipmap> newMip = mu::Clone<ASTOpImageMipmap>(currentMipmapOp);
			newMip->Levels = blockLevels;
			newMip->BlockLevels = blockLevels;
			newMip->bOnlyTail = false;

			auto newOp = mu::Clone<ASTOpImagePatch>(at);
			newOp->base = Visit(newOp->base.child(), newMip.get());
			newOp->patch = Visit(newOp->patch.child(), newMip.get());
			newAt = newOp;

			// We need to add a mipmap on top to finish the mipmapping
			{
				mu::Ptr<ASTOpImageMipmap> topMipOp = mu::Clone<ASTOpImageMipmap>(currentMipmapOp);
				topMipOp->Source = newOp;
				topMipOp->bOnlyTail = true;
				newAt = topMipOp;
			}
		}

		break;
	}

	default:
		break;
	}

	// end on line, replace with mipmap
	if (at == newAt && at != m_initialSource)
	{
		mu::Ptr<ASTOpImageMipmap> newOp = mu::Clone<ASTOpImageMipmap>(currentMipmapOp);
		check(newOp->GetOpType() == OP_TYPE::IM_MIPMAP);

		newOp->Source = at;

		newAt = newOp;
	}

	m_oldToNew.Add(at, newAt);

	return newAt;
}

}