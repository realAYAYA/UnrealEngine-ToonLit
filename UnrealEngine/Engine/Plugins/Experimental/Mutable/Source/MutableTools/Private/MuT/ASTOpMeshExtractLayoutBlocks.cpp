// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshExtractLayoutBlocks.h"

#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "Misc/AssertionMacros.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	ASTOpMeshExtractLayoutBlocks::ASTOpMeshExtractLayoutBlocks()
		: Source(this)
	{
	}


	ASTOpMeshExtractLayoutBlocks::~ASTOpMeshExtractLayoutBlocks()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshExtractLayoutBlocks::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshExtractLayoutBlocks* other = static_cast<const ASTOpMeshExtractLayoutBlocks*>(&otherUntyped);
			return Source == other->Source && Layout == other->Layout && Blocks == other->Blocks;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpMeshExtractLayoutBlocks::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshExtractLayoutBlocks> n = new ASTOpMeshExtractLayoutBlocks();
		n->Source = mapChild(Source.child());
		n->Layout = Layout;
		n->Blocks = Blocks;
		return n;
	}


	void ASTOpMeshExtractLayoutBlocks::Assert()
	{
		check(Blocks.Num() < std::numeric_limits<uint16>::max());
		ASTOp::Assert();
	}


	void ASTOpMeshExtractLayoutBlocks::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	uint64 ASTOpMeshExtractLayoutBlocks::Hash() const
	{
		uint64 res = std::hash<size_t>()(size_t(Source.child().get()));
		return res;
	}


	void ASTOpMeshExtractLayoutBlocks::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_EXTRACTLAYOUTBLOCK);
			OP::ADDRESS sourceAt = Source ? Source->linkedAddress : 0;
			AppendCode(program.m_byteCode, sourceAt);
			AppendCode(program.m_byteCode, (uint16)Layout);
			AppendCode(program.m_byteCode, (uint16)Blocks.Num());

			for (auto b : Blocks)
			{
				AppendCode(program.m_byteCode, (uint32)b);
			}
		}
	}


	mu::Ptr<ASTOp> ASTOpMeshExtractLayoutBlocks::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		if (!Source.child())
		{
			return nullptr;
		}

		OP_TYPE SourceType = Source.child()->GetOpType();

		// Optimize only the mesh parameter
		switch (SourceType)
		{

		case OP_TYPE::ME_SWITCH:
		{
			// Move the operation down all the paths
			Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(Source.child());

			if (NewSwitch->def)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> NewBind = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				NewBind->Source = NewSwitch->def.child();
				NewSwitch->def = NewBind;
			}

			for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
			{
				if (NewSwitch->cases[v].branch)
				{
					Ptr<ASTOpMeshExtractLayoutBlocks> NewBind = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
					NewBind->Source = NewSwitch->cases[v].branch.child();
					NewSwitch->cases[v].branch = NewBind;
				}
			}

			NewOp = NewSwitch;
			break;
		}

		case OP_TYPE::ME_CONDITIONAL:
		{
			// Move the operation down all the paths
			Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(Source.child());

			if (NewConditional->yes)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> NewBind = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				NewBind->Source = NewConditional->yes.child();
				NewConditional->yes = NewBind;
			}

			if (NewConditional->no)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> NewBind = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				NewBind->Source = NewConditional->no.child();
				NewConditional->no = NewBind;
			}

			NewOp = NewConditional;
			break;
		}

		case OP_TYPE::ME_ADDTAGS:
		{
			Ptr<ASTOpMeshAddTags> NewAddTags = mu::Clone<ASTOpMeshAddTags>(Source.child());

			if (NewAddTags->Source)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> New = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				New->Source = NewAddTags->Source.child();
				NewAddTags->Source = New;
			}

			NewOp = NewAddTags;
			break;
		}

		case OP_TYPE::ME_MORPH:
		{
			// Move the operation down the base and the target
			Ptr<ASTOpMeshMorph> NewMorph = mu::Clone<ASTOpMeshMorph>(Source.child());

			if (NewMorph->Base)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> New = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				New->Source = NewMorph->Base.child();
				NewMorph->Base = New;
			}

			if (NewMorph->Target)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> New = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				New->Source = NewMorph->Target.child();
				NewMorph->Target = New;
			}

			NewOp = NewMorph;
			break;
		}

		case OP_TYPE::ME_REMOVEMASK:
		{
			// Move the operation down the base
			// \TODO: mask too to try to make them smaller?
			Ptr<ASTOpMeshRemoveMask> NewRemove = mu::Clone<ASTOpMeshRemoveMask>(Source.child());

			if (NewRemove->source)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> New = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				New->Source = NewRemove->source.child();
				NewRemove->source = New;
			}

			NewOp = NewRemove;
			break;
		}

		case OP_TYPE::ME_CLIPMORPHPLANE:
		{
			// Move the operation down the source
			Ptr<ASTOpMeshClipMorphPlane> NewMorph = mu::Clone<ASTOpMeshClipMorphPlane>(Source.child());

			if (NewMorph->source)
			{
				Ptr<ASTOpMeshExtractLayoutBlocks> New = mu::Clone<ASTOpMeshExtractLayoutBlocks>(this);
				New->Source = NewMorph->source.child();
				NewMorph->source = New;
			}

			NewOp = NewMorph;
			break;
		}

		default:
			break;

		}

		return NewOp;
	}


}
