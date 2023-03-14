// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImagePatch.h"

#include "Containers/Map.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	ASTOpImagePatch::ASTOpImagePatch()
		: base(this)
		, patch(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImagePatch::~ASTOpImagePatch()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImagePatch::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpImagePatch*>(&otherUntyped))
		{
			return base == other->base &&
				patch == other->patch &&
				location == other->location;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImagePatch::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(OP_TYPE::IM_PATCH);
		hash_combine(res, base.child().get());
		hash_combine(res, patch.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImagePatch::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImagePatch> n = new ASTOpImagePatch();
		n->base = mapChild(base.child());
		n->patch = mapChild(patch.child());
		n->location = location;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImagePatch::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(base);
		f(patch);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImagePatch::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImagePatchArgs args;
			memset(&args, 0, sizeof(args));

			if (base) args.base = base->linkedAddress;
			if (patch) args.patch = patch->linkedAddress;
			args.minX = location[0];
			args.minY = location[1];

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::IM_PATCH);
			AppendCode(program.m_byteCode, args);
		}

	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImagePatch::GetImageDesc(bool returnBestOption, GetImageDescContext* context)
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
	mu::Ptr<ImageSizeExpression> ASTOpImagePatch::GetImageSizeExpression() const
	{
		if (base)
		{
			return base->GetImageSizeExpression();
		}

		return nullptr;
	}


}
