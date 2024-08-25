// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpLayoutRemoveBlocks.h"

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
	ASTOpLayoutRemoveBlocks::ASTOpLayoutRemoveBlocks()
		: Source(this)
		, ReferenceLayout(this)
	{
	}


	ASTOpLayoutRemoveBlocks::~ASTOpLayoutRemoveBlocks()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpLayoutRemoveBlocks::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpLayoutRemoveBlocks* other = static_cast<const ASTOpLayoutRemoveBlocks*>(&otherUntyped);
			return Source == other->Source && ReferenceLayout == other->ReferenceLayout;
		}
		return false;
	}


	uint64 ASTOpLayoutRemoveBlocks::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, ReferenceLayout.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpLayoutRemoveBlocks::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpLayoutRemoveBlocks> n = new ASTOpLayoutRemoveBlocks();
		n->Source = mapChild(Source.child());
		n->ReferenceLayout = mapChild(ReferenceLayout.child());
		return n;
	}


	void ASTOpLayoutRemoveBlocks::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
		f(ReferenceLayout);
	}


	void ASTOpLayoutRemoveBlocks::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::LayoutRemoveBlocksArgs args;
			FMemory::Memzero(&args, sizeof(args));

			if (Source) args.Source = Source->linkedAddress;
			if (ReferenceLayout) args.ReferenceLayout = ReferenceLayout->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::LA_REMOVEBLOCKS);
			AppendCode(program.m_byteCode, args);
		}

	}


	void ASTOpLayoutRemoveBlocks::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache)
	{
		if (Source)
		{
			Source->GetBlockLayoutSize(blockIndex, pBlockX, pBlockY, cache);
		}
	}


}
