// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshApplyShape.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	ASTOpMeshApplyShape::ASTOpMeshApplyShape()
		: Mesh(this)
		, Shape(this)
		, bReshapeSkeleton(false)
		, bReshapePhysicsVolumes(false)
		, bReshapeVertices(true)
		, bApplyLaplacian(false)
	{
	}


	ASTOpMeshApplyShape::~ASTOpMeshApplyShape()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshApplyShape::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshApplyShape* Other = static_cast<const ASTOpMeshApplyShape*>(&OtherUntyped);

			const bool bSameFlags =
				bReshapePhysicsVolumes == Other->bReshapePhysicsVolumes &&
				bReshapeSkeleton == Other->bReshapeSkeleton &&
				bReshapeVertices == Other->bReshapeVertices &&
				bApplyLaplacian == Other->bApplyLaplacian;

			return Mesh == Other->Mesh && Shape == Other->Shape && bSameFlags;
		}
		return false;
	}


	uint64 ASTOpMeshApplyShape::Hash() const
	{
		uint64 Result = std::hash<void*>()(Mesh.child().get());
		hash_combine(Result, Shape.child().get());
		hash_combine(Result, bool(bReshapeSkeleton));
		hash_combine(Result, bool(bReshapePhysicsVolumes));
		hash_combine(Result, bool(bReshapeVertices));
		hash_combine(Result, bool(bApplyLaplacian));

		return Result;
	}


	mu::Ptr<ASTOp> ASTOpMeshApplyShape::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshApplyShape> NewOp = new ASTOpMeshApplyShape();
		NewOp->Mesh = MapChild(Mesh.child());
		NewOp->Shape = MapChild(Shape.child());
		NewOp->bReshapeSkeleton = bReshapeSkeleton;
		NewOp->bReshapePhysicsVolumes = bReshapePhysicsVolumes;
		NewOp->bReshapeVertices = bReshapeVertices;
		NewOp->bApplyLaplacian = bApplyLaplacian;
		return NewOp;
	}


	void ASTOpMeshApplyShape::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Mesh);
		Func(Shape);
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
			EnumAddFlags(BindFlags, bApplyLaplacian ? EMeshBindShapeFlags::ApplyLaplacian : NoFlags);

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
