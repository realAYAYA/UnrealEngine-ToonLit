// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpLayoutPack.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/StreamsPrivate.h"



namespace mu
{


	//-------------------------------------------------------------------------------------------------
	ASTOpLayoutPack::ASTOpLayoutPack()
		: Source(this)
	{
	}


	ASTOpLayoutPack::~ASTOpLayoutPack()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpLayoutPack::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpLayoutPack* other = static_cast<const ASTOpLayoutPack*>(&otherUntyped);
			return Source == other->Source;
		}
		return false;
	}


	uint64 ASTOpLayoutPack::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpLayoutPack::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpLayoutPack> n = new ASTOpLayoutPack();
		n->Source = mapChild(Source.child());
		return n;
	}


	void ASTOpLayoutPack::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	void ASTOpLayoutPack::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::LayoutRemoveBlocksArgs args;
			FMemory::Memzero(&args, sizeof(args));

			if (Source) args.Source = Source->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}

	}


	void ASTOpLayoutPack::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache)
	{
		if (Source)
		{
			Source->GetBlockLayoutSize(blockIndex, pBlockX, pBlockY, cache);
		}
	}


}
