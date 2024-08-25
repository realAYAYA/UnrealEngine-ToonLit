// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMorph.h"

#include "MuT/StreamsPrivate.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "HAL/PlatformMath.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	ASTOpMeshMorph::ASTOpMeshMorph()
		: Factor(this), Base(this), Target(this)
	{
	}


	//---------------------------------------------------------------------------------------------
	ASTOpMeshMorph::~ASTOpMeshMorph()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//---------------------------------------------------------------------------------------------
	bool ASTOpMeshMorph::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshMorph* other = static_cast<const ASTOpMeshMorph*>(&otherUntyped);
			return Factor == other->Factor && Base == other->Base && Target == other->Target;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMorph::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshMorph> n = new ASTOpMeshMorph();
		n->Factor = mapChild(Factor.child());
		n->Base = mapChild(Base.child());
		n->Target = mapChild(Target.child());
		return n;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshMorph::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Factor);
		f(Base);
		f(Target);
	}


	//---------------------------------------------------------------------------------------------
	uint64 ASTOpMeshMorph::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Factor.child().get());
		hash_combine(res, Base.child().get());
		hash_combine(res, Target.child().get());
		return res;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshMorph::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_MORPH);

			OP::ADDRESS FactorAt = Factor ? Factor->linkedAddress : 0;
			AppendCode(program.m_byteCode, FactorAt);

			OP::ADDRESS BaseAt = Base ? Base->linkedAddress : 0;
			AppendCode(program.m_byteCode, BaseAt);

			OP::ADDRESS TargetAt = Target ? Target->linkedAddress : 0;
			AppendCode(program.m_byteCode, TargetAt);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMorph::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		if (!Base.child())
		{
			return nullptr;
		}

		// Base optimizations
		OP_TYPE BaseType = Base.child()->GetOpType();
		switch (BaseType)
		{

		case OP_TYPE::ME_ADDTAGS:
		{
			// Add the base tags after morphing
			Ptr<ASTOpMeshAddTags> NewAddTags = mu::Clone<ASTOpMeshAddTags>(Base.child());

			if (NewAddTags->Source)
			{
				Ptr<ASTOpMeshMorph> New = mu::Clone<ASTOpMeshMorph>(this);
				New->Base = NewAddTags->Source.child();
				NewAddTags->Source = New;
			}

			NewOp = NewAddTags;
			break;
		}

		default:
			break;

		}

		// If not optimized yet
		if (!NewOp)
		{
			// Target optimizations
			OP_TYPE MorphType = Target.child()->GetOpType();
			switch (MorphType)
			{

			case OP_TYPE::ME_ADDTAGS:
			{
				// Ignore the morph target tags
				const ASTOpMeshAddTags* AddTags = static_cast<const ASTOpMeshAddTags*>(Target.child().get());

				Ptr<ASTOpMeshMorph> New = mu::Clone<ASTOpMeshMorph>(this);
				New->Target = AddTags->Source.child();
				NewOp = New;

				break;
			}

			default:
				break;

			}
		}

		return NewOp;
	}

}
