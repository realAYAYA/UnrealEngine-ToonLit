// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshTransform.h"

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

	ASTOpMeshTransform::ASTOpMeshTransform()
		: source(this)
	{
	}


	ASTOpMeshTransform::~ASTOpMeshTransform()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshTransform::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshTransform*>(&otherUntyped))
		{
			return source == other->source &&
				matrix == other->matrix;
		}
		return false;
	}


	uint64 ASTOpMeshTransform::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(OP_TYPE::ME_TRANSFORM);
		hash_combine(res, source.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshTransform::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshTransform> n = new ASTOpMeshTransform();
		n->matrix = matrix;
		n->source = mapChild(source.child());
		return n;
	}


	void ASTOpMeshTransform::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
	}


	void ASTOpMeshTransform::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshTransformArgs args;
			memset(&args, 0, sizeof(args));

			if (source) args.source = source->linkedAddress;

			args.matrix = program.AddConstant(matrix);

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_TRANSFORM);
			AppendCode(program.m_byteCode, args);
		}

	}

}
