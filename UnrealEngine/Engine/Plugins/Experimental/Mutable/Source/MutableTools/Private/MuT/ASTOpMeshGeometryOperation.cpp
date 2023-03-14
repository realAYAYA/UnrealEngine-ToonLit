// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshGeometryOperation.h"

#include "HAL/PlatformMath.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>

namespace mu
{

	ASTOpMeshGeometryOperation::ASTOpMeshGeometryOperation()
		: meshA(this)
		, meshB(this)
		, scalarA(this)
		, scalarB(this)
	{
	}


	ASTOpMeshGeometryOperation::~ASTOpMeshGeometryOperation()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshGeometryOperation::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshGeometryOperation*>(&otherUntyped))
		{
			return meshA == other->meshA && meshB == other->meshB &&
				scalarA == other->scalarA && scalarB == other->scalarB;
		}
		return false;
	}


	uint64 ASTOpMeshGeometryOperation::Hash() const
	{
		uint64 res = std::hash<void*>()(meshA.child().get());
		hash_combine(res, meshB.child().get());
		hash_combine(res, scalarA.child().get());
		hash_combine(res, scalarB.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshGeometryOperation::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshGeometryOperation> n = new ASTOpMeshGeometryOperation();
		n->meshA = mapChild(meshA.child());
		n->meshB = mapChild(meshB.child());
		n->scalarA = mapChild(scalarA.child());
		n->scalarB = mapChild(scalarB.child());
		return n;
	}


	void ASTOpMeshGeometryOperation::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(meshA);
		f(meshB);
		f(scalarA);
		f(scalarB);
	}


	void ASTOpMeshGeometryOperation::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshGeometryOperationArgs args;
			memset(&args, 0, sizeof(args));

			if (meshA) args.meshA = meshA->linkedAddress;
			if (meshB) args.meshB = meshB->linkedAddress;
			if (scalarA) args.scalarA = scalarA->linkedAddress;
			if (scalarB) args.scalarB = scalarB->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_GEOMETRYOPERATION);
			AppendCode(program.m_byteCode, args);
		}

	}

}
