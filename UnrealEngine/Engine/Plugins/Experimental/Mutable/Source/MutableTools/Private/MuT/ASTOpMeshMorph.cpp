// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMorph.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>

namespace mu
{

	//---------------------------------------------------------------------------------------------
	ASTOpMeshMorph::ASTOpMeshMorph()
		: Factor(this), Base(this)
	{
	}


	//---------------------------------------------------------------------------------------------
	ASTOpMeshMorph::~ASTOpMeshMorph()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshMorph::AddTarget(const Ptr<ASTOp>& InTarget)
	{
		Targets.Add( ASTChild(this,InTarget));
	}


	//---------------------------------------------------------------------------------------------
	bool ASTOpMeshMorph::IsEqual(const ASTOp& otherUntyped) const
	{
		if (const ASTOpMeshMorph* other = dynamic_cast<const ASTOpMeshMorph*>(&otherUntyped))
		{
			return Factor == other->Factor && Base == other->Base && Targets == other->Targets;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMorph::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshMorph> n = new ASTOpMeshMorph();
		n->Factor = mapChild(Factor.child());
		n->Base = mapChild(Base.child());
		for (const ASTChild& r : Targets)
		{
			n->Targets.Add(ASTChild(n,mapChild(r.child())));
		}
		return n;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshMorph::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Factor);
		f(Base);
		for (ASTChild& T : Targets)
		{
			f(T);
		}
	}


	//---------------------------------------------------------------------------------------------
	uint64 ASTOpMeshMorph::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Factor.child().get());
		hash_combine(res, Base.child().get());
		for (const ASTChild& T : Targets)
		{
			hash_combine(res, T.child().get());
		}
		return res;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshMorph::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_MORPH2);

			OP::ADDRESS FactorAt = Factor ? Factor->linkedAddress : 0;
			AppendCode(program.m_byteCode, FactorAt);

			OP::ADDRESS BaseAt = Base ? Base->linkedAddress : 0;
			AppendCode(program.m_byteCode, BaseAt);

			AppendCode(program.m_byteCode, (uint8)Targets.Num());
			for (const ASTChild& b : Targets)
			{
				OP::ADDRESS TargetAt = b ? b->linkedAddress : 0;
				AppendCode(program.m_byteCode, TargetAt);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMorph::OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const
	{
		return nullptr;
	}

}
