// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshApplyShape.h"

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


	ASTOpMeshApplyShape::ASTOpMeshApplyShape()
		: Mesh(this)
		, Shape(this)
	{
	}


	ASTOpMeshApplyShape::~ASTOpMeshApplyShape()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshApplyShape::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshApplyShape*>(&otherUntyped))
		{
			const bool bSameFlags =
				m_reshapePhysicsVolumes == other->m_reshapePhysicsVolumes &&
				m_reshapeSkeleton == other->m_reshapeSkeleton &&
				m_reshapeVertices == other->m_reshapeVertices;

			return Mesh == other->Mesh && Shape == other->Shape && bSameFlags;
		}
		return false;
	}


	uint64 ASTOpMeshApplyShape::Hash() const
	{
		uint64 res = std::hash<void*>()(Mesh.child().get());
		hash_combine(res, Shape.child().get());
		hash_combine(res, m_reshapeSkeleton);
		hash_combine(res, m_reshapePhysicsVolumes);
		hash_combine(res, m_reshapeVertices);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshApplyShape::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshApplyShape> n = new ASTOpMeshApplyShape();
		n->Mesh = mapChild(Mesh.child());
		n->Shape = mapChild(Shape.child());
		n->m_reshapeSkeleton = m_reshapeSkeleton;
		n->m_reshapePhysicsVolumes = m_reshapePhysicsVolumes;
		n->m_reshapeVertices = m_reshapeVertices;
		return n;
	}


	void ASTOpMeshApplyShape::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Mesh);
		f(Shape);
	}


	void ASTOpMeshApplyShape::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshApplyShapeArgs args;
			memset(&args, 0, sizeof(args));

			if (m_reshapeSkeleton)
			{
				args.flags |= uint32(OP::EMeshBindShapeFlags::ReshapeSkeleton);
			}

			if (m_reshapePhysicsVolumes)
			{
				args.flags |= uint32(OP::EMeshBindShapeFlags::ReshapePhysicsVolumes);
			}

			if (m_reshapeVertices)
			{
				args.flags |= uint32(OP::EMeshBindShapeFlags::ReshapeVertices);
			}


			if (Mesh) args.mesh = Mesh->linkedAddress;
			if (Shape) args.shape = Shape->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_APPLYSHAPE);
			AppendCode(program.m_byteCode, args);
		}

	}

}
