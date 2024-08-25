// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshAddTags.h"

#include "MuT/ASTOpMeshMorph.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	ASTOpMeshAddTags::ASTOpMeshAddTags()
		: Source(this)
	{
	}


	ASTOpMeshAddTags::~ASTOpMeshAddTags()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshAddTags::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshAddTags* other = static_cast<const ASTOpMeshAddTags*>(&otherUntyped);
			return Source == other->Source && Tags == other->Tags;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpMeshAddTags::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshAddTags> n = new ASTOpMeshAddTags();
		n->Source = mapChild(Source.child());
		n->Tags = Tags;
		return n;
	}


	void ASTOpMeshAddTags::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	uint64 ASTOpMeshAddTags::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Source.child().get());
		return res;
	}


	void ASTOpMeshAddTags::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)Program.m_opAddress.Num();

			Program.m_opAddress.Add((uint32)Program.m_byteCode.Num());
			AppendCode(Program.m_byteCode, OP_TYPE::ME_ADDTAGS);
			OP::ADDRESS SourceAt = Source ? Source->linkedAddress : 0;
			AppendCode(Program.m_byteCode, SourceAt);
			AppendCode(Program.m_byteCode, (uint16)Tags.Num());
			for (const FString& Tag : Tags)
			{
				OP::ADDRESS TagConstantAddress = Program.AddConstant(Tag);
				AppendCode(Program.m_byteCode, TagConstantAddress);
			}
		}
	}

}
