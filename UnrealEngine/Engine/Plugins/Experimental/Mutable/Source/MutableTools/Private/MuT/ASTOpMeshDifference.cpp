// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshDifference.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpMeshRemoveMask.h"

#include <memory>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	ASTOpMeshDifference::ASTOpMeshDifference()
		: Base(this), Target(this)
	{
	}


	//---------------------------------------------------------------------------------------------
	ASTOpMeshDifference::~ASTOpMeshDifference()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//---------------------------------------------------------------------------------------------
	bool ASTOpMeshDifference::IsEqual(const ASTOp& otherUntyped) const
	{
		if (const ASTOpMeshDifference* Other = dynamic_cast<const ASTOpMeshDifference*>(&otherUntyped))
		{
			return Base == Other->Base && Target == Other->Target
				&& bIgnoreTextureCoords == Other->bIgnoreTextureCoords
				&& Channels == Other->Channels;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshDifference::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshDifference> n = new ASTOpMeshDifference();
		n->Base = mapChild(Base.child());
		n->Target = mapChild(Target.child());
		n->bIgnoreTextureCoords = bIgnoreTextureCoords;
		n->Channels = Channels;		
		return n;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshDifference::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Base);
		f(Target);
	}


	//---------------------------------------------------------------------------------------------
	uint64 ASTOpMeshDifference::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Base.child().get());
		hash_combine(res, Target.child().get());
		return res;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshDifference::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_DIFFERENCE);

			OP::ADDRESS BaseAt = Base ? Base->linkedAddress : 0;
			AppendCode(program.m_byteCode, BaseAt);

			OP::ADDRESS TargetAt = Target ? Target->linkedAddress : 0;
			AppendCode(program.m_byteCode, TargetAt);

			AppendCode(program.m_byteCode, (uint8)bIgnoreTextureCoords);

			AppendCode(program.m_byteCode, (uint8)Channels.Num());
			for (const FChannel& b : Channels)
			{
				AppendCode(program.m_byteCode, b.Semantic);
				AppendCode(program.m_byteCode, b.SemanticIndex);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshDifference::OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const
	{
		Ptr<ASTOp> NewOp;

		Ptr<ASTOp> BaseAt = Base.child();
		if (!BaseAt)
		{
			return nullptr;
		}

		Ptr<ASTOp> TargetAt = Target.child();
		if (!TargetAt)
		{
			return nullptr;
		}

		OP_TYPE BaseType = BaseAt->GetOpType();
		OP_TYPE TargetType = TargetAt->GetOpType();

		// See if both base and target have an operation that can be optimized in a combined way
		if (BaseType == TargetType)
		{
			switch (BaseType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// If the switch variable and structure is the same
				const ASTOpSwitch* BaseSwitch = reinterpret_cast<const ASTOpSwitch*>(BaseAt.get());
				const ASTOpSwitch* TargetSwitch = reinterpret_cast<const ASTOpSwitch*>(TargetAt.get());
				bool bIsSimilarSwitch = BaseSwitch->IsCompatibleWith(TargetSwitch);
				if (!bIsSimilarSwitch)
				{
					break;
				}

				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(BaseAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseSwitch->def.child();
					NewDiff->Target = TargetSwitch->def.child();
					NewSwitch->def = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Base = BaseSwitch->cases[v].branch.child();
						NewDiff->Target = TargetSwitch->FindBranch(BaseSwitch->cases[v].condition);
						NewSwitch->cases[v].branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}


			case OP_TYPE::ME_CONDITIONAL:
			{
				const ASTOpConditional* BaseConditional = reinterpret_cast<const ASTOpConditional*>(BaseAt.get());
				const ASTOpConditional* TargetConditional = reinterpret_cast<const ASTOpConditional*>(TargetAt.get());
				bool bIsSimilar = BaseConditional->condition == TargetConditional->condition;
				if (!bIsSimilar)
				{
					break;
				}

				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(BaseAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseConditional->yes.child();
					NewDiff->Target = TargetConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseConditional->no.child();
					NewDiff->Target = TargetConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}


			default:
				break;

			}
		}


		// If not already optimized
		if (!NewOp)
		{
			// Optimize only the mesh parameter
			switch (BaseType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(BaseAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewSwitch->def.child();
					NewSwitch->def = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Base = NewSwitch->cases[v].branch.child();
						NewSwitch->cases[v].branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case OP_TYPE::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(BaseAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}

			default:
				break;

			}
		}

		// If not already optimized
		if (!NewOp)
		{
			// Optimize only the shape parameter
			switch (TargetType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(TargetAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewSwitch->def.child();
					NewSwitch->def = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Target = NewSwitch->cases[v].branch.child();
						NewSwitch->cases[v].branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case OP_TYPE::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(TargetAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}

			default:
				break;

			}
		}


		return NewOp;
	}

}
