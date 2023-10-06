// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshApplyShape.h"

#include "HAL/PlatformMath.h"
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
		, bReshapeSkeleton(false)
		, bReshapePhysicsVolumes(false)
		, bReshapeVertices(true)
	{
	}


	ASTOpMeshApplyShape::~ASTOpMeshApplyShape()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshApplyShape::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (const ASTOpMeshApplyShape* Other = dynamic_cast<const ASTOpMeshApplyShape*>(&OtherUntyped))
		{
			const bool bSameFlags =
				bReshapePhysicsVolumes == Other->bReshapePhysicsVolumes &&
				bReshapeSkeleton == Other->bReshapeSkeleton &&
				bReshapeVertices == Other->bReshapeVertices;

			return Mesh == Other->Mesh && Shape == Other->Shape && bSameFlags;
		}
		return false;
	}


	uint64 ASTOpMeshApplyShape::Hash() const
	{
		uint64 res = std::hash<void*>()(Mesh.child().get());
		hash_combine(res, Shape.child().get());
		hash_combine(res, bool(bReshapeSkeleton));
		hash_combine(res, bool(bReshapePhysicsVolumes));
		hash_combine(res, bool(bReshapeVertices));
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshApplyShape::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshApplyShape> NewOp = new ASTOpMeshApplyShape();
		NewOp->Mesh = mapChild(Mesh.child());
		NewOp->Shape = mapChild(Shape.child());
		NewOp->bReshapeSkeleton = bReshapeSkeleton;
		NewOp->bReshapePhysicsVolumes = bReshapePhysicsVolumes;
		NewOp->bReshapeVertices = bReshapeVertices;
		return NewOp;
	}


	void ASTOpMeshApplyShape::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Mesh);
		f(Shape);
	}


	void ASTOpMeshApplyShape::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshApplyShapeArgs Args;
			FMemory::Memzero(&Args, sizeof(Args));

			constexpr EMeshBindShapeFlags NoFlags = EMeshBindShapeFlags::None;
			EMeshBindShapeFlags BindFlags = NoFlags;
			EnumAddFlags(BindFlags, bReshapeSkeleton ? EMeshBindShapeFlags::ReshapeSkeleton : NoFlags);
			EnumAddFlags(BindFlags, bReshapePhysicsVolumes ? EMeshBindShapeFlags::ReshapePhysicsVolumes : NoFlags);
			EnumAddFlags(BindFlags, bReshapeVertices ? EMeshBindShapeFlags::ReshapeVertices : NoFlags);

			Args.flags = static_cast<uint32>(BindFlags);

			Args.mesh = Mesh ? Mesh->linkedAddress : 0;
			Args.shape = Shape ? Shape->linkedAddress : 0;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_APPLYSHAPE);
			AppendCode(program.m_byteCode, Args);
		}

	}

}
