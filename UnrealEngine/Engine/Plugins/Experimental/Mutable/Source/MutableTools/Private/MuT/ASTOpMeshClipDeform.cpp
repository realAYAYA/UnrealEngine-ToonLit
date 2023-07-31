// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshClipDeform.h"

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


	ASTOpMeshClipDeform::ASTOpMeshClipDeform()
		: Mesh(this)
		, ClipShape(this)
	{
	}


	ASTOpMeshClipDeform::~ASTOpMeshClipDeform()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshClipDeform::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (const ASTOpMeshClipDeform* Other = dynamic_cast<const ASTOpMeshClipDeform*>(&OtherUntyped))
		{
			return Mesh == Other->Mesh && ClipShape == Other->ClipShape;
		}

		return false;
	}


	uint64 ASTOpMeshClipDeform::Hash() const
	{
		uint64 res = std::hash<void*>()(Mesh.child().get());
		hash_combine(res, ClipShape.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshClipDeform::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshClipDeform> n = new ASTOpMeshClipDeform();
		n->Mesh = mapChild(Mesh.child());
		n->ClipShape = mapChild(ClipShape.child());
		return n;
	}


	void ASTOpMeshClipDeform::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Mesh);
		f(ClipShape);
	}


	void ASTOpMeshClipDeform::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshClipDeformArgs args;
			memset(&args, 0, sizeof(args));

			if (Mesh)
			{
				args.mesh = Mesh->linkedAddress;
			}

			if (ClipShape)
			{
				args.clipShape = ClipShape->linkedAddress;
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_CLIPDEFORM);
			AppendCode(program.m_byteCode, args);
		}

	}

}
