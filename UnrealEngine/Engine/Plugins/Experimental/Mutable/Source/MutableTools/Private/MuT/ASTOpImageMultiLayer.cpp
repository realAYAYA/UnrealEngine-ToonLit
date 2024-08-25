// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageMultiLayer.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	ASTOpImageMultiLayer::ASTOpImageMultiLayer()
		: base(this)
		, blend(this)
		, mask(this)
		, range(this, nullptr, FString(), FString())
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageMultiLayer::~ASTOpImageMultiLayer()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageMultiLayer::IsEqual(const ASTOp& InOtherUntyped) const
	{
		if (InOtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageMultiLayer* Other = static_cast<const ASTOpImageMultiLayer*>(&InOtherUntyped);
			return base == Other->base &&
				blend == Other->blend &&
				mask == Other->mask &&
				range == Other->range &&
				blendType == Other->blendType &&
				blendTypeAlpha == Other->blendTypeAlpha &&
				BlendAlphaSourceChannel == Other->BlendAlphaSourceChannel &&
				bUseMaskFromBlended == Other->bUseMaskFromBlended;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageMultiLayer::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(GetOpType());
		hash_combine(res, base.child().get());
		hash_combine(res, blend.child().get());
		hash_combine(res, mask.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageMultiLayer::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageMultiLayer> n = new ASTOpImageMultiLayer();
		n->base = mapChild(base.child());
		n->blend = mapChild(blend.child());
		n->mask = mapChild(mask.child());
		n->range.rangeName = range.rangeName;
		n->range.rangeUID = range.rangeUID;
		n->range.rangeSize = mapChild(range.rangeSize.child());
		n->blendType = blendType;
		n->blendTypeAlpha = blendTypeAlpha;
		n->BlendAlphaSourceChannel = BlendAlphaSourceChannel;
		n->bUseMaskFromBlended = bUseMaskFromBlended;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageMultiLayer::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(base);
		f(blend);
		f(mask);
		f(range.rangeSize);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageMultiLayer::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageMultiLayerArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.blendType = (uint8)blendType;
			args.blendTypeAlpha = (uint8)blendTypeAlpha;
			args.BlendAlphaSourceChannel = BlendAlphaSourceChannel;
			args.bUseMaskFromBlended = bUseMaskFromBlended;

			if (base) args.base = base->linkedAddress;
			if (blend) args.blended = blend->linkedAddress;
			if (mask) args.mask = mask->linkedAddress;
			if (range.rangeSize)
			{
				LinkRange(program, range, args.rangeSize, args.rangeId);
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageMultiLayer::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const 
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

		// Actual work
		if (base)
		{
			res = base->GetImageDesc(returnBestOption, context);
		}


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageMultiLayer::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (base)
		{
			base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpImageMultiLayer::GetImageSizeExpression() const
	{
		if (base)
		{
			return base->GetImageSizeExpression();
		}

		return nullptr;
	}

}
