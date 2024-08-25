// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshBindShape.h"

#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	ASTOpMeshBindShape::ASTOpMeshBindShape()
		: Mesh(this)
		, Shape(this)
		, bReshapeSkeleton(false)
		, bReshapePhysicsVolumes(false)
		, bReshapeVertices(false)
		, bApplyLaplacian(false)
	{
	}


	ASTOpMeshBindShape::~ASTOpMeshBindShape()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshBindShape::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshBindShape* Other = static_cast<const ASTOpMeshBindShape*>(&OtherUntyped);

			const bool bSameFlags =
				bReshapeSkeleton == Other->bReshapeSkeleton	&&
				bReshapePhysicsVolumes == Other->bReshapePhysicsVolumes &&
				bReshapeVertices == Other->bReshapeVertices &&
				bApplyLaplacian == Other->bApplyLaplacian;

			return bSameFlags &&
				Mesh == Other->Mesh &&
				Shape == Other->Shape &&
				BonesToDeform == Other->BonesToDeform &&
				PhysicsToDeform == Other->PhysicsToDeform &&
				BindingMethod == Other->BindingMethod &&
				RChannelUsage == Other->RChannelUsage && 
				GChannelUsage == Other->GChannelUsage && 
				BChannelUsage == Other->BChannelUsage && 
				AChannelUsage == Other->AChannelUsage;
		}

		return false;
	}


	uint64 ASTOpMeshBindShape::Hash() const
	{
		uint64 Result = std::hash<void*>()(Mesh.child().get());
		hash_combine(Result, Shape.child().get());
		hash_combine(Result, bool(bReshapeSkeleton));
		hash_combine(Result, bool(bReshapePhysicsVolumes));
		hash_combine(Result, bool(bReshapeVertices));
		hash_combine(Result, bool(bApplyLaplacian));
		hash_combine(Result, bool(BindingMethod));

		hash_combine(Result, static_cast<uint32>(RChannelUsage));
		hash_combine(Result, static_cast<uint32>(GChannelUsage));
		hash_combine(Result, static_cast<uint32>(BChannelUsage));
		hash_combine(Result, static_cast<uint32>(AChannelUsage));

		for (const uint16 S : BonesToDeform)
		{
			hash_combine(Result, S);
		}

		for (const uint16 S : PhysicsToDeform)
		{
			hash_combine(Result, S);
		}

		return Result;
	}


	mu::Ptr<ASTOp> ASTOpMeshBindShape::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshBindShape> NewOp = new ASTOpMeshBindShape();
		NewOp->Mesh = mapChild(Mesh.child());
		NewOp->Shape = mapChild(Shape.child());
		NewOp->bReshapeSkeleton	= bReshapeSkeleton;
		NewOp->bReshapePhysicsVolumes = bReshapePhysicsVolumes;
		NewOp->bReshapeVertices = bReshapeVertices;
		NewOp->bApplyLaplacian = bApplyLaplacian;
		NewOp->BonesToDeform = BonesToDeform;
		NewOp->PhysicsToDeform = PhysicsToDeform;
		NewOp->BindingMethod = BindingMethod;

		NewOp->RChannelUsage = RChannelUsage;
		NewOp->GChannelUsage = GChannelUsage;
		NewOp->BChannelUsage = BChannelUsage;
		NewOp->AChannelUsage = AChannelUsage;

		return NewOp;
	}


	void ASTOpMeshBindShape::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Mesh);
		Func(Shape);
	}


	void ASTOpMeshBindShape::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshBindShapeArgs Args;
			FMemory::Memzero(&Args, sizeof(Args));

			constexpr EMeshBindShapeFlags NoFlags = EMeshBindShapeFlags::None;
			EMeshBindShapeFlags BindFlags = NoFlags;
			EnumAddFlags(BindFlags, bReshapeSkeleton ? EMeshBindShapeFlags::ReshapeSkeleton : NoFlags);
			EnumAddFlags(BindFlags, bReshapePhysicsVolumes ? EMeshBindShapeFlags::ReshapePhysicsVolumes : NoFlags);
			EnumAddFlags(BindFlags, bReshapeVertices ? EMeshBindShapeFlags::ReshapeVertices : NoFlags);
			EnumAddFlags(BindFlags, bApplyLaplacian ? EMeshBindShapeFlags::ApplyLaplacian : NoFlags);

			{
				auto ConvertColorUsage = [](EVertexColorUsage Usage)
				{
					switch (Usage)
					{
					case EVertexColorUsage::None:			   return EMeshBindColorChannelUsage::None;
					case EVertexColorUsage::ReshapeClusterId:  return EMeshBindColorChannelUsage::ClusterId;
					case EVertexColorUsage::ReshapeMaskWeight: return EMeshBindColorChannelUsage::MaskWeight;
					default: check(false); return EMeshBindColorChannelUsage::None;
					};
				};
	
				const FMeshBindColorChannelUsages ColorUsages = {
					ConvertColorUsage(RChannelUsage),
					ConvertColorUsage(GChannelUsage),
					ConvertColorUsage(BChannelUsage),
					ConvertColorUsage(AChannelUsage) };

				FMemory::Memcpy(&Args.ColorUsage, &ColorUsages, sizeof(Args.ColorUsage));
				static_assert(sizeof(Args.ColorUsage) == sizeof(ColorUsages));
			}

			Args.flags = static_cast<uint32>(BindFlags);

			Args.bindingMethod = BindingMethod;

			if (Mesh)
			{
				Args.mesh = Mesh->linkedAddress;
			}

			if (Shape)
			{
				Args.shape = Shape->linkedAddress;
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_BINDSHAPE);
			AppendCode(program.m_byteCode, Args);

			AppendCode(program.m_byteCode, (int32)BonesToDeform.Num());
			for (const uint16 S : BonesToDeform)
			{
				AppendCode(program.m_byteCode, S);
			}

			AppendCode(program.m_byteCode, (int32)PhysicsToDeform.Num());
			for (const uint16 S : PhysicsToDeform)
			{
				AppendCode(program.m_byteCode, S);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshBindShape::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		Ptr<ASTOp> MeshAt = Mesh.child();
		if (!MeshAt)
		{
			return nullptr;
		}

		Ptr<ASTOp> ShapeAt = Shape.child();
		if (!ShapeAt)
		{
			return nullptr;
		}

		OP_TYPE MeshType = MeshAt->GetOpType();
		OP_TYPE ShapeType = ShapeAt->GetOpType();

		// See if both mesh and shape have an operation that can be optimized in a combined way
		if (MeshType == ShapeType)
		{
			switch (MeshType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// If the switch variable and structure is the same
				const ASTOpSwitch* MeshSwitch = static_cast<const ASTOpSwitch*>(MeshAt.get());
				const ASTOpSwitch* ShapeSwitch = static_cast<const ASTOpSwitch*>(ShapeAt.get());
				bool bIsSimilarSwitch = MeshSwitch->IsCompatibleWith(ShapeSwitch);
				if (!bIsSimilarSwitch)
				{
					break;
				}

				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(MeshAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = MeshSwitch->def.child();
					NewBind->Shape = ShapeSwitch->def.child();
					NewSwitch->def = NewBind;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
						NewBind->Mesh = MeshSwitch->cases[v].branch.child();
						NewBind->Shape = ShapeSwitch->FindBranch(MeshSwitch->cases[v].condition);
						NewSwitch->cases[v].branch = NewBind;
					}
				}

				NewOp = NewSwitch;
				break;
			}


			case OP_TYPE::ME_CONDITIONAL:
			{
				const ASTOpConditional* MeshConditional = static_cast<const ASTOpConditional*>(MeshAt.get());
				const ASTOpConditional* ShapeConditional = static_cast<const ASTOpConditional*>(ShapeAt.get());
				bool bIsSimilar = MeshConditional->condition == ShapeConditional->condition;
				if (!bIsSimilar)
				{
					break;
				}

				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(MeshAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = MeshConditional->yes.child();
					NewBind->Shape = ShapeConditional->yes.child();
					NewConditional->yes = NewBind;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = MeshConditional->no.child();
					NewBind->Shape = ShapeConditional->no.child();
					NewConditional->no = NewBind;
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
			switch (MeshType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(MeshAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = NewSwitch->def.child();
					NewSwitch->def = NewBind;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
						NewBind->Mesh = NewSwitch->cases[v].branch.child();
						NewSwitch->cases[v].branch = NewBind;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case OP_TYPE::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(MeshAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = NewConditional->yes.child();
					NewConditional->yes = NewBind;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = NewConditional->no.child();
					NewConditional->no = NewBind;
				}

				NewOp = NewConditional;
				break;
			}

			case OP_TYPE::ME_REMOVEMASK:
			{
				// We bind something that could have a part removed: we can reorder to bind the entire mesh
				// and apply remove later at runtime.

				Ptr<ASTOpMeshRemoveMask> NewRemove = mu::Clone<ASTOpMeshRemoveMask>(MeshAt);
				if (NewRemove->source)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = NewRemove->source.child();
					NewRemove->source = NewBind;
				}

				NewOp = NewRemove;
				break;
			}

			case OP_TYPE::ME_ADDTAGS:
			{
				Ptr<ASTOpMeshAddTags> New = mu::Clone<ASTOpMeshAddTags>(MeshAt);
				if (New->Source)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Mesh = New->Source.child();
					New->Source = NewBind;
				}

				NewOp = New;
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
			switch (ShapeType)
			{

			case OP_TYPE::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(ShapeAt);

				if (NewSwitch->def)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Shape = NewSwitch->def.child();
					NewSwitch->def = NewBind;
				}

				for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
				{
					if (NewSwitch->cases[v].branch)
					{
						Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
						NewBind->Shape = NewSwitch->cases[v].branch.child();
						NewSwitch->cases[v].branch = NewBind;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case OP_TYPE::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(ShapeAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Shape = NewConditional->yes.child();
					NewConditional->yes = NewBind;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
					NewBind->Shape = NewConditional->no.child();
					NewConditional->no = NewBind;
				}

				NewOp = NewConditional;
				break;
			}

			case OP_TYPE::ME_ADDTAGS:
			{
				// Ignore the tags in the shape
				Ptr<ASTOpMeshBindShape> NewBind = mu::Clone<ASTOpMeshBindShape>(this);
				const ASTOpMeshAddTags* New = static_cast<const ASTOpMeshAddTags*>(ShapeAt.get());
				NewBind->Shape = New->Source.child();
				NewOp = NewBind;
				break;
			}

			default:
				break;

			}
		}


		return NewOp;
	}

}
