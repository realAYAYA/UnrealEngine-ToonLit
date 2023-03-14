// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageTransform.h"

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

	//-------------------------------------------------------------------------------------------------
	ASTOpImageTransform::ASTOpImageTransform()
		: base(this)
		, offsetX(this)
		, offsetY(this)
		, scaleX(this)
		, scaleY(this)
		, rotation(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageTransform::~ASTOpImageTransform()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageTransform::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpImageTransform*>(&otherUntyped))
		{
			return base == other->base &&
				offsetX == other->offsetX &&
				offsetY == other->offsetY &&
				scaleX == other->scaleX &&
				scaleY == other->scaleY &&
				rotation == other->rotation;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageTransform::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(OP_TYPE::IM_MULTILAYER);
		hash_combine(res, base.child().get());
		hash_combine(res, offsetX.child().get());
		hash_combine(res, offsetY.child().get());
		hash_combine(res, scaleX.child().get());
		hash_combine(res, scaleY.child().get());
		hash_combine(res, rotation.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageTransform::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageTransform> n = new ASTOpImageTransform();
		n->base = mapChild(base.child());
		n->offsetX = mapChild(offsetX.child());
		n->offsetY = mapChild(offsetY.child());
		n->scaleX = mapChild(scaleX.child());
		n->scaleY = mapChild(scaleY.child());
		n->rotation = mapChild(rotation.child());
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageTransform::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(base);
		f(offsetX);
		f(offsetY);
		f(scaleX);
		f(scaleY);
		f(rotation);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageTransform::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageTransformArgs Args;

			Args.base = base ? base->linkedAddress : 0;
			Args.offsetX = offsetX ? offsetX->linkedAddress : 0;
			Args.offsetY = offsetY ? offsetY->linkedAddress : 0;
			Args.scaleX = scaleX ? scaleX->linkedAddress : 0;
			Args.scaleY = scaleY ? scaleY->linkedAddress : 0;
			Args.rotation = rotation ? rotation->linkedAddress : 0;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::IM_TRANSFORM);
			AppendCode(program.m_byteCode, Args);
		}
	}

	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageTransform::GetImageDesc(bool returnBestOption, GetImageDescContext* context)
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
	void ASTOpImageTransform::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (base)
		{
			base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpImageTransform::GetImageSizeExpression() const
	{
		if (base)
		{
			return base->GetImageSizeExpression();
		}

		return nullptr;
	}

}
