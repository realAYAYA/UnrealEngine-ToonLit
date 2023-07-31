// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageMultiLayer.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	ASTOpImageMultiLayer::ASTOpImageMultiLayer()
		: base(this)
		, blend(this)
		, mask(this)
		, range(this, nullptr, string(), string())
		, blendType(EBlendType::BT_BLEND)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageMultiLayer::~ASTOpImageMultiLayer()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageMultiLayer::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpImageMultiLayer*>(&otherUntyped))
		{
			return base == other->base &&
				blend == other->blend &&
				mask == other->mask &&
				range == other->range &&
				blendType == other->blendType;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageMultiLayer::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(OP_TYPE::IM_MULTILAYER);
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
	void ASTOpImageMultiLayer::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageMultiLayerArgs args;
			memset(&args, 0, sizeof(args));

			args.blendType = (uint16)blendType;

			if (base) args.base = base->linkedAddress;
			if (blend) args.blended = blend->linkedAddress;
			if (mask) args.mask = mask->linkedAddress;
			if (range.rangeSize)
			{
				LinkRange(program, range, args.rangeSize, args.rangeId);
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::IM_MULTILAYER);
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageMultiLayer::GetImageDesc(bool returnBestOption, GetImageDescContext* context)
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
