// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpParameter.h"

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Parameters.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


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
	}


	bool ASTOpParameter::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpParameter* other = static_cast<const ASTOpParameter*>(&otherUntyped);
			return type == other->type &&
				parameter == other->parameter &&
				ranges == other->ranges;
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
		return n;
	}


	uint64 ASTOpParameter::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(type));
		hash_combine(res, parameter.m_type);
		hash_combine(res, parameter.m_name.Len());
		return res;
	}


	void ASTOpParameter::Assert()
	{
		ASTOp::Assert();
	}


	void ASTOpParameter::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ParameterArgs args;
			FMemory::Memzero(&args, sizeof(args));

			LinkedParameterIndex = Program.m_parameters.Find(parameter);

			// If this fails, it means an ASTOpParameter was created at code generation time, but not registered into the
			// parameters map in the CodeGenerator_FirstPass.
			check(LinkedParameterIndex != INDEX_NONE);

			args.variable = (OP::ADDRESS)LinkedParameterIndex;

			for (const auto& d : ranges)
			{
				OP::ADDRESS sizeAt = 0;
				uint16 rangeId = 0;
				LinkRange(Program, d, sizeAt, rangeId);
				Program.m_parameters[LinkedParameterIndex].m_ranges.Add(rangeId);
			}

			linkedAddress = (OP::ADDRESS)Program.m_opAddress.Num();

			Program.m_opAddress.Add((uint32)Program.m_byteCode.Num());
			AppendCode(Program.m_byteCode, type);
			AppendCode(Program.m_byteCode, args);
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
				const auto typedFact = static_cast<const ASTOpFixed*>(f.get());
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


	ASTOp::FBoolEvalResult ASTOpParameter::EvaluateBool(ASTOpList& /*facts*/,
		FEvaluateBoolCache*) const
	{
		check(type == OP_TYPE::BO_PARAMETER);
		return BET_UNKNOWN;
	}


	FImageDesc ASTOpParameter::GetImageDesc(bool, FGetImageDescContext*) const
	{
		check(type == OP_TYPE::IM_PARAMETER);
		return FImageDesc();
	}

}
