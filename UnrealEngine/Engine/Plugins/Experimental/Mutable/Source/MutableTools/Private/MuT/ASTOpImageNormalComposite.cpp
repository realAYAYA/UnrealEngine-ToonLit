// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageNormalComposite.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	ASTOpImageNormalComposite::ASTOpImageNormalComposite()
		: Base(this)
		, Normal(this)
		, Mode(ECompositeImageMode::CIM_Disabled)
		, Power(1.0f)
	{
	}


	ASTOpImageNormalComposite::~ASTOpImageNormalComposite()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageNormalComposite::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (const ASTOpImageNormalComposite* Other = dynamic_cast<const ASTOpImageNormalComposite*>(&OtherUntyped))
		{
			return Base == Other->Base && Normal == Other->Normal && Power == Other->Power && Mode == Other->Mode;
		}

		return false;
	}


	uint64 ASTOpImageNormalComposite::Hash() const
	{
		uint64 Res = std::hash<void*>()(Base.child().get());
		hash_combine(Res, Normal.child().get());

		return Res;
	}


	mu::Ptr<ASTOp> ASTOpImageNormalComposite::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageNormalComposite> N = new ASTOpImageNormalComposite();
		N->Base = mapChild(Base.child());
		N->Normal = mapChild(Normal.child());
		N->Mode = Mode;
		N->Power = Power;

		return N;
	}


	void ASTOpImageNormalComposite::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Base);
		f(Normal);
	}


	void ASTOpImageNormalComposite::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageNormalCompositeArgs args;
			FMemory::Memset(&args, 0, sizeof(args));

			if (Base)
			{
				args.base = Base->linkedAddress;
			}

			if (Normal)
			{
				args.normal = Normal->linkedAddress;
			}

			args.power = Power;
			args.mode = Mode;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::IM_NORMALCOMPOSITE);
			AppendCode(program.m_byteCode, args);
		}

	}


	FImageDesc ASTOpImageNormalComposite::GetImageDesc(bool returnBestOption, GetImageDescContext* context)
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

		if (Base)
		{
			res = Base->GetImageDesc(returnBestOption, context);
		}

		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}

	void ASTOpImageNormalComposite::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (Base)
		{
			Base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}

	mu::Ptr<ImageSizeExpression> ASTOpImageNormalComposite::GetImageSizeExpression() const
	{
		if (Base)
		{
			return Base->GetImageSizeExpression();
		}

		return nullptr;
	}

}
