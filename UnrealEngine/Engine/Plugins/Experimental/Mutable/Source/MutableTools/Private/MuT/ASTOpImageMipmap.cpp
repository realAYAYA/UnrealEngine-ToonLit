// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageMipmap.h"

#include "Containers/Map.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
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
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageMipmap* other = static_cast<const ASTOpImageMipmap*>(&otherUntyped);
			return Source == other->Source &&
				Levels == other->Levels &&
				BlockLevels == other->BlockLevels &&
				bOnlyTail == other->bOnlyTail &&
				bPreventSplitTail == other->bPreventSplitTail &&
				SharpenFactor == other->SharpenFactor &&
				AddressMode == other->AddressMode &&
				FilterType == other->FilterType &&
				DitherMipmapAlpha == other->DitherMipmapAlpha;
		}
		return false;
	}


	uint64 ASTOpImageMipmap::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, Levels);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImageMipmap::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpImageMipmap> n = new ASTOpImageMipmap();
		n->Source = mapChild(Source.child());
		n->Levels = Levels;
		n->BlockLevels = BlockLevels;
		n->bOnlyTail = bOnlyTail;
		n->bPreventSplitTail = bPreventSplitTail;
		n->SharpenFactor = SharpenFactor;
		n->AddressMode = AddressMode;
		n->FilterType = FilterType;
		n->DitherMipmapAlpha = DitherMipmapAlpha;

		return n;
	}


	void ASTOpImageMipmap::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	void ASTOpImageMipmap::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageMipmapArgs args;
			FMemory::Memzero(&args, sizeof(args));

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
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	mu::Ptr<ASTOp> ASTOpImageMipmap::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
	{
		mu::Ptr<ASTOp> at;

		// \TODO: This seems to fail with the bandit test model.
		//if (Source.child())
		//{
		//	ASTOp::FGetImageDescContext context;
		//	FImageDesc ChildDesc = Source.child()->GetImageDesc(false, &context);

		//	if (ChildDesc.m_lods>0 && ChildDesc.m_lods>=Levels)
		//	{
		//		// We can skip the mipmaps, because the child will always contain all lods anyway.
		//		at = Source.child();
		//	}
		//}

		return at;
	}


	mu::Ptr<ASTOp> ASTOpImageMipmap::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		mu::Ptr<ASTOp> at;

		mu::Ptr<ASTOp> sourceAt = Source.child();
		switch (sourceAt->GetOpType())
		{

		case OP_TYPE::IM_BLANKLAYOUT:
		{
			// Set the mipmap generation on the blank layout operation
			Ptr<ASTOpFixed> mop = mu::Clone<ASTOpFixed>(sourceAt);
			mop->op.args.ImageBlankLayout.generateMipmaps = 1;    // true
			mop->op.args.ImageBlankLayout.mipmapCount = Levels;
			at = mop;
			break;
		}

		case OP_TYPE::IM_PLAINCOLOUR:
		{
			// Set the mipmap generation on the plaincolour operation
			Ptr<ASTOpFixed> mop = mu::Clone<ASTOpFixed>(sourceAt);
			mop->op.args.ImagePlainColour.LODs = Levels;
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
	FImageDesc ASTOpImageMipmap::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const
	{
		FImageDesc res;

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

		if (Source.child())
		{
			res = Source.child()->GetImageDesc(returnBestOption, context);
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


	bool ASTOpImageMipmap::IsImagePlainConstant(FVector4f& colour) const
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
	mu::Ptr<ASTOp> Sink_ImageMipmapAST::Apply(const ASTOpImageMipmap* InRoot)
	{
		m_root = InRoot;
		OldToNew.Empty();

		if (m_root->bOnlyTail)
		{
			return nullptr;
		}

		mu::Ptr<ASTOp> newSource;
		m_initialSource = m_root->Source.child();

		// Before sinking, see if it is worth splitting into miptail and mip.
		if (!m_root->bPreventSplitTail
			// \TODO: Review this: It seems it fails for a BC1 8x4 texture having blockSize=4, and level=0
			&&
			// No need to split if there is no difference between the levels considering blocks
			m_root->Levels != m_root->BlockLevels 
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

		// Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at,currentMipmapOp });
		if (Cached)
		{
			return *Cached;
		}

		mu::Ptr<ASTOp> newAt = at;
		switch (at->GetOpType())
		{

		case OP_TYPE::IM_CONDITIONAL:
		{
			// We move the op down the two paths
			Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
			newOp->yes = Visit(newOp->yes.child(), currentMipmapOp);
			newOp->no = Visit(newOp->no.child(), currentMipmapOp);
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_SWITCH:
		{
			// We move the op down all the paths
			Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
			newOp->def = Visit(newOp->def.child(), currentMipmapOp);
			for (ASTOpSwitch::FCase& c : newOp->cases)
			{
				c.branch = Visit(c.branch.child(), currentMipmapOp);
			}
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_COMPOSE:
		{
			const ASTOpImageCompose* typedAt = static_cast<const ASTOpImageCompose*>(at.get());
			if (!currentMipmapOp->bOnlyTail
				&&
				// Don't move the mipmapping if we are composing with a mask.
				// TODO: allow mipmapping in the masks, RLE formats, etc.
				!typedAt->Mask
				)
			{
				Ptr<ASTOpImageCompose> newOp = mu::Clone<ASTOpImageCompose>(at);

				Ptr<ASTOp> baseOp = newOp->Base.child();
				newOp->Base = Visit(baseOp, currentMipmapOp);

				Ptr<ASTOp> blockOp = newOp->BlockImage.child();
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

				const ASTOpImagePatch* typedSource = static_cast<const ASTOpImagePatch*>(at.get());
				Ptr<ASTOp> rectOp = typedSource->patch.child();
				ASTOp::FGetImageDescContext context;
				FImageDesc patchDesc = rectOp->GetImageDesc(false, &context);

				// Calculate the mip levels that can be calculated for the patch
				uint8 PatchBlockLevels = 0;
				{
					uint16 minX = typedSource->location[0];
					uint16 minY = typedSource->location[1];
					uint16 sizeX = patchDesc.m_size[0];
					uint16 sizeY = patchDesc.m_size[1];
					while (minX && minY && sizeX && sizeY
						&&
						minX % 2 == 0 && minY % 2 == 0 && sizeX % 2 == 0 && sizeY % 2 == 0)
					{
						PatchBlockLevels++;
						minX /= 2;
						minY /= 2;
						sizeX /= 2;
						sizeY /= 2;
					}
				}

				if (currentMipmapOp->Levels!= PatchBlockLevels || currentMipmapOp->BlockLevels!= PatchBlockLevels)
				{
					mu::Ptr<ASTOpImageMipmap> newMip = mu::Clone<ASTOpImageMipmap>(currentMipmapOp);
					newMip->Levels = PatchBlockLevels;
					newMip->BlockLevels = PatchBlockLevels;
					newMip->bOnlyTail = false;

					Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);
					newOp->base = Visit(newOp->base.child(), newMip.get());
					newOp->patch = Visit(newOp->patch.child(), newMip.get());
					newAt = newOp;

					if (currentMipmapOp->Levels != currentMipmapOp->BlockLevels						
						// If the current levels are all of them, we will want to rebuild the mips after patch.
						// This happens if ignoring layouts, in which case there is no top-most mipmap to ensure the tail already.
						|| currentMipmapOp->BlockLevels == 0 
						)
					{
						// We need to add a mipmap on top to finish the mipmapping
						mu::Ptr<ASTOpImageMipmap> topMipOp = mu::Clone<ASTOpImageMipmap>(currentMipmapOp);
						topMipOp->Source = newOp;
						topMipOp->bOnlyTail = true;
						newAt = topMipOp;
					}
				}
				else
				{
					// The patch supports the same amount of mips that we are currently sinking.
					Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);
					newOp->base = Visit(newOp->base.child(), currentMipmapOp);
					newOp->patch = Visit(newOp->patch.child(), currentMipmapOp);
					newAt = newOp;
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

		OldToNew.Add({ at,currentMipmapOp }, newAt);

		return newAt;
	}

}