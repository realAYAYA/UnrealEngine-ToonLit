// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConditional.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpConditional::ASTOpConditional()
		: condition(this)
		, yes(this)
		, no(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpConditional::~ASTOpConditional()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConditional::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpConditional*>(&otherUntyped))
		{
			return type == other->type && condition == other->condition &&
				yes == other->yes && no == other->no;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpConditional::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpConditional> n = new ASTOpConditional();
		n->type = type;
		n->condition = mapChild(condition.child());
		n->yes = mapChild(yes.child());
		n->no = mapChild(no.child());
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpConditional::Hash() const
	{
		uint64 res = std::hash<void*>()(condition.m_child.get());
		hash_combine(res, yes.m_child.get());
		hash_combine(res, no.m_child.get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConditional::Assert()
	{
		switch (type)
		{
		case OP_TYPE::NU_CONDITIONAL:
		case OP_TYPE::SC_CONDITIONAL:
		case OP_TYPE::CO_CONDITIONAL:
		case OP_TYPE::IM_CONDITIONAL:
		case OP_TYPE::ME_CONDITIONAL:
		case OP_TYPE::LA_CONDITIONAL:
		case OP_TYPE::IN_CONDITIONAL:
			break;
		default:
			// Unexpected type
			check(false);
			break;
		}

		ASTOp::Assert();
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConditional::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(condition);
		f(yes);
		f(no);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConditional::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ConditionalArgs args;
			memset(&args, 0, sizeof(args));

			if (condition) args.condition = condition->linkedAddress;
			if (yes) args.yes = yes->linkedAddress;
			if (no) args.no = no->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, type);
			AppendCode(program.m_byteCode, args);

		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpConditional::GetImageDesc(bool returnBestOption, class GetImageDescContext* context)
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

		if (type == OP_TYPE::IM_CONDITIONAL)
		{
			// In a conditional we cannot guarantee the size and format.
			// We check both options, and if they are the same we return that.
			// Otherwise, we return an empty descriptor that will force re-formatting of the image.
			// The code optimiser will take care then of moving the format operations down to each
			// branch and remove the unnecessary ones.
			FImageDesc noDesc;
			FImageDesc yesDesc;

			if (no.child())
			{
				noDesc = no->GetImageDesc(returnBestOption, context);
			}
			if (yes.child())
			{
				yesDesc = yes->GetImageDesc(returnBestOption, context);
			}

			if (yesDesc == noDesc || returnBestOption)
			{
				res = yesDesc;
			}
			else
			{
				res.m_format = (yesDesc.m_format == noDesc.m_format) ? yesDesc.m_format : EImageFormat::IF_NONE;
				res.m_lods = (yesDesc.m_lods == noDesc.m_lods) ? yesDesc.m_lods : 0;
				res.m_size = (yesDesc.m_size == noDesc.m_size) ? yesDesc.m_size : FImageSize(0, 0);
			}
		}
		else
		{
			checkf(false, TEXT("Instruction not supported"));
		}


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConditional::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (type == OP_TYPE::IM_CONDITIONAL)
		{
			yes->GetLayoutBlockSize(pBlockX, pBlockY);

			if (!*pBlockX)
			{
				no->GetLayoutBlockSize(pBlockX, pBlockY);
			}
		}
		else
		{
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConditional::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY,
		BLOCK_LAYOUT_SIZE_CACHE* cache)
	{
		if (type == OP_TYPE::LA_CONDITIONAL)
		{
			yes->GetBlockLayoutSizeCached(blockIndex, pBlockX, pBlockY, cache);

			if (*pBlockX == 0)
			{
				no->GetBlockLayoutSizeCached(blockIndex, pBlockX, pBlockY, cache);
			}
		}
		else
		{
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConditional::GetNonBlackRect(FImageRect& maskUsage) const
	{
		if (type == OP_TYPE::IM_CONDITIONAL)
		{
			FImageRect local;
			bool localValid = false;
			if (yes)
			{
				localValid = yes->GetNonBlackRect(local);
				if (!localValid)
				{
					return false;
				}
			}

			if (no)
			{
				FImageRect noRect;
				bool validNo = no->GetNonBlackRect(noRect);
				if (validNo)
				{
					if (localValid)
					{
						local.Bound(noRect);
					}
					else
					{
						local = noRect;
						localValid = true;
					}
				}
				else
				{
					return false;
				}
			}

			if (localValid)
			{
				maskUsage = local;
				return true;
			}
		}

		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpConditional::OptimiseSemantic(const MODEL_OPTIMIZATION_OPTIONS&) const
	{
		if (!condition)
		{
			// If there is no expression, we'll assume true.
			return yes.child();
		}

		// If the branches are the same, remove the instruction
		if (yes.child() == no.child())
		{
			return yes.child();
		}

		// Constant condition?
		if (condition->GetOpType() == OP_TYPE::BO_CONSTANT)
		{
			auto typedCondition = dynamic_cast<const ASTOpConstantBool*>(condition.child().get());
			if (typedCondition->value)
			{
				return yes.child();
			}

			return no.child();
		}

		else
		{
			// If the yes branch is a conditional with the same condition
			if (yes && yes->GetOpType() == type)
			{
				auto typedYes = dynamic_cast<const ASTOpConditional*>(yes.child().get());
				if (condition.child() == typedYes->condition.child()
					||
					*condition.child() == *typedYes->condition.child())
				{
					auto op = mu::Clone<ASTOpConditional>(this);
					op->yes = typedYes->yes.child();
					return op;
				}
			}

			// If the no branch is a conditional with the same condition
			else if (no && no->GetOpType() == type)
			{
				auto typedNo = dynamic_cast<const ASTOpConditional*>(no.child().get());
				if (condition.child() == typedNo->condition.child()
					||
					*condition.child() == *typedNo->condition.child())
				{
					auto op = mu::Clone<ASTOpConditional>(this);
					op->no = typedNo->no.child();
					return op;
				}
			}
		}

		return nullptr;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpConditional::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
		pRes->type = ImageSizeExpression::ISET_CONDITIONAL;
		pRes->yes = yes->GetImageSizeExpression();
		pRes->no = no->GetImageSizeExpression();

		if (*pRes->yes == *pRes->no)
		{
			pRes = pRes->yes;
		}

		return pRes;
	}

}