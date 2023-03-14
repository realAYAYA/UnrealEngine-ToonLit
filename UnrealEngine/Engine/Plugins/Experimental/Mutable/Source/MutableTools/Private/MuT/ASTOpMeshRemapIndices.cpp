// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshRemapIndices.h"

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


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshRemapIndices::ASTOpMeshRemapIndices()
		: source(this)
		, reference(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpMeshRemapIndices::~ASTOpMeshRemapIndices()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpMeshRemapIndices::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshRemapIndices*>(&otherUntyped))
		{
			return source == other->source && reference == other->reference;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpMeshRemapIndices::Hash() const
	{
		uint64 res = std::hash<void*>()(source.child().get());
		hash_combine(res, reference.child());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshRemapIndices::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshRemapIndices> n = new ASTOpMeshRemapIndices();
		n->source = mapChild(source.child());
		n->reference = mapChild(reference.child());
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshRemapIndices::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
		f(reference);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshRemapIndices::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshRemapIndicesArgs args;
			memset(&args, 0, sizeof(args));

			if (source) args.source = source->linkedAddress;
			if (reference) args.reference = reference->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_REMAPINDICES);
			AppendCode(program.m_byteCode, args);
		}
	}

}
