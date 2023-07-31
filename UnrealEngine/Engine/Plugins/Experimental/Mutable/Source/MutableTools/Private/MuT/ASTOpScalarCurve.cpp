// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpScalarCurve.h"

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


	ASTOpScalarCurve::ASTOpScalarCurve()
		: time(this)
	{
	}


	ASTOpScalarCurve::~ASTOpScalarCurve()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	void ASTOpScalarCurve::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(time);
	}


	uint64 ASTOpScalarCurve::Hash() const
	{
		uint64 res = std::hash<uint64>()(size_t(OP_TYPE::SC_CURVE));
		hash_combine(res, curve.keyFrames.Num());
		return res;
	}


	bool ASTOpScalarCurve::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpScalarCurve*>(&otherUntyped))
		{
			return time == other->time && curve == other->curve;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpScalarCurve::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpScalarCurve> n = new ASTOpScalarCurve();
		n->curve = curve;
		n->time = mapChild(time.child());
		return n;
	}


	void ASTOpScalarCurve::Link(PROGRAM& program, const FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ScalarCurveArgs args;
			memset(&args, 0, sizeof(args));
			args.time = time ? time->linkedAddress : 0;
			args.curve = program.AddConstant(curve);

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::SC_CURVE);
			AppendCode(program.m_byteCode, args);
		}

	}

}

