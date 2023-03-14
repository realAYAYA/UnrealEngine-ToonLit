// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMorphReshape.h"

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


	ASTOpMeshMorphReshape::ASTOpMeshMorphReshape()
		: Morph(this)
		, Reshape(this)
	{
	}


	ASTOpMeshMorphReshape::~ASTOpMeshMorphReshape()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshMorphReshape::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshMorphReshape*>(&otherUntyped))
		{
			return Morph == other->Morph && Reshape == other->Reshape;
		}

		return false;
	}


	uint64 ASTOpMeshMorphReshape::Hash() const
	{
		uint64 res = std::hash<void*>()(Morph.child().get());
		hash_combine(res, Reshape.child().get());

		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshMorphReshape::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshMorphReshape> n = new ASTOpMeshMorphReshape();
		n->Morph = mapChild(Morph.child());
		n->Reshape = mapChild(Reshape.child());
		return n;
	}


	void ASTOpMeshMorphReshape::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Morph);
		f(Reshape);
	}


	void ASTOpMeshMorphReshape::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshMorphReshapeArgs Args;
			memset(&Args, 0, sizeof(Args));

			if (Morph)
			{
				Args.Morph = Morph->linkedAddress;
			}

			if (Reshape)
			{
				Args.Reshape = Reshape->linkedAddress;
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_MORPHRESHAPE);
			AppendCode(program.m_byteCode, Args);
		}

	}

}
