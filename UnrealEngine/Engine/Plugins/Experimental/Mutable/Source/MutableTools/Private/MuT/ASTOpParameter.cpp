// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpParameter.h"

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Parameters.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	ASTOpParameter::~ASTOpParameter()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	void ASTOpParameter::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		for (auto& c : ranges)
		{
			f(c.rangeSize);
		}
		for (auto& c : additionalImages)
		{
			f(c);
		}
	}


	bool ASTOpParameter::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpParameter*>(&otherUntyped))
		{
			return type == other->type &&
				parameter == other->parameter &&
				ranges == other->ranges &&
				additionalImages == other->additionalImages;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpParameter::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpParameter> n = new ASTOpParameter();
		n->type = type;
		n->parameter = parameter;
		for (const auto& c : ranges)
		{
			n->ranges.Emplace(n.get(), mapChild(c.rangeSize.child()), c.rangeName, c.rangeUID);
		}
		for (const auto& c : additionalImages)
		{
			n->additionalImages.Emplace(n, mapChild(c.child()));
		}
		return n;
	}


	uint64 ASTOpParameter::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(type));
		hash_combine(res, parameter.m_type);
		hash_combine(res, parameter.m_name);
		return res;
	}


	void ASTOpParameter::Assert()
	{
		ASTOp::Assert();
	}


	void ASTOpParameter::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ParameterArgs args;
			memset(&args, 0, sizeof(args));

			args.variable = (OP::ADDRESS)program.m_parameters.Num();
			program.m_parameters.Add(parameter);

			for (const auto& d : ranges)
			{
				OP::ADDRESS sizeAt = 0;
				uint16 rangeId = 0;
				LinkRange(program, d, sizeAt, rangeId);
				program.m_parameters.Last().m_ranges.Add(rangeId);
			}

			for (const auto& d : additionalImages)
			{
				OP::ADDRESS descAt = d ? d->linkedAddress : 0;
				program.m_parameters.Last().m_descImages.Add(descAt);
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);

			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, type);
			AppendCode(program.m_byteCode, args);
		}
	}


	int ASTOpParameter::EvaluateInt(ASTOpList& facts, bool& unknown) const
	{
		unknown = true;

		// Check the facts, in case we have the value for our parameter.
		for (const auto& f : facts)
		{
			if (f->GetOpType() == OP_TYPE::BO_EQUAL_INT_CONST)
			{
				const auto typedFact = dynamic_cast<const ASTOpFixed*>(f.get());
				auto value = typedFact->children[typedFact->op.args.BoolEqualScalarConst.value].child();
				if (value.get() == this)
				{
					unknown = false;
					return typedFact->op.args.BoolEqualScalarConst.constant;
				}
				else
				{
					// We could try something more if it was an expression and it had the parameter
					// somewhere in it.
				}
			}
		}

		return 0;
	}


	ASTOp::BOOL_EVAL_RESULT ASTOpParameter::EvaluateBool(ASTOpList& /*facts*/,
		EVALUATE_BOOL_CACHE*) const
	{
		check(type == OP_TYPE::BO_PARAMETER);
		return BET_UNKNOWN;
	}


	FImageDesc ASTOpParameter::GetImageDesc(bool, GetImageDescContext*)
	{
		check(type == OP_TYPE::IM_PARAMETER);
		return FImageDesc();
	}

}
