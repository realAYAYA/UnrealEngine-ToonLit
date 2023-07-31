// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshExtractLayoutBlocks.h"

#include "Misc/AssertionMacros.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

#include <memory>
#include <utility>

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	ASTOpMeshExtractLayoutBlocks::ASTOpMeshExtractLayoutBlocks()
		: source(this)
	{
	}


	ASTOpMeshExtractLayoutBlocks::~ASTOpMeshExtractLayoutBlocks()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshExtractLayoutBlocks::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshExtractLayoutBlocks*>(&otherUntyped))
		{
			return source == other->source && layout == other->layout && blocks == other->blocks;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpMeshExtractLayoutBlocks::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshExtractLayoutBlocks> n = new ASTOpMeshExtractLayoutBlocks();
		n->source = mapChild(source.child());
		n->layout = layout;
		n->blocks = blocks;
		return n;
	}


	void ASTOpMeshExtractLayoutBlocks::Assert()
	{
		check(blocks.Num() < std::numeric_limits<uint16>::max());
		ASTOp::Assert();
	}


	void ASTOpMeshExtractLayoutBlocks::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
	}


	uint64 ASTOpMeshExtractLayoutBlocks::Hash() const
	{
		uint64 res = std::hash<size_t>()(size_t(source.child().get()));
		return res;
	}


	void ASTOpMeshExtractLayoutBlocks::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_EXTRACTLAYOUTBLOCK);
			OP::ADDRESS sourceAt = source ? source->linkedAddress : 0;
			AppendCode(program.m_byteCode, sourceAt);
			AppendCode(program.m_byteCode, (uint16)layout);
			AppendCode(program.m_byteCode, (uint16)blocks.Num());

			for (auto b : blocks)
			{
				AppendCode(program.m_byteCode, (uint32)b);
			}
		}
	}

}
