// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshOptimizeSkinning.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshOptimizeSkinning::ASTOpMeshOptimizeSkinning()
		: source(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpMeshOptimizeSkinning::~ASTOpMeshOptimizeSkinning()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpMeshOptimizeSkinning::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshOptimizeSkinning* other = static_cast<const ASTOpMeshOptimizeSkinning*>(&otherUntyped);
			return source == other->source;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpMeshOptimizeSkinning::Hash() const
	{
		uint64 res = std::hash<void*>()(source.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshOptimizeSkinning::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshOptimizeSkinning> n = new ASTOpMeshOptimizeSkinning();
		n->source = mapChild(source.child());
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshOptimizeSkinning::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshOptimizeSkinning::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshOptimizeSkinningArgs args;
			FMemory::Memset(&args, 0, sizeof(args));

			if (source) args.source = source->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_OPTIMIZESKINNING);
			AppendCode(program.m_byteCode, args);
		}
	}

}
