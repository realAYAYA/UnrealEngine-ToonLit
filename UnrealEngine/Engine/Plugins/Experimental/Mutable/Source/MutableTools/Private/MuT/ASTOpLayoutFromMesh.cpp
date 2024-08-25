// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpLayoutFromMesh.h"

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
#include "MuT/ASTOpMeshFormat.h"
#include "MuT/ASTOpMeshApplyShape.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpLayoutMerge.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	ASTOpLayoutFromMesh::ASTOpLayoutFromMesh()
		: Mesh(this)
	{
	}


	ASTOpLayoutFromMesh::~ASTOpLayoutFromMesh()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpLayoutFromMesh::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpLayoutFromMesh* other = static_cast<const ASTOpLayoutFromMesh*>(&otherUntyped);
			return Mesh == other->Mesh && LayoutIndex == other->LayoutIndex;
		}
		return false;
	}


	uint64 ASTOpLayoutFromMesh::Hash() const
	{
		uint64 res = std::hash<void*>()(Mesh.child().get());
		hash_combine(res, LayoutIndex);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpLayoutFromMesh::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpLayoutFromMesh> n = new ASTOpLayoutFromMesh();
		n->Mesh = mapChild(Mesh.child());
		n->LayoutIndex = LayoutIndex;
		return n;
	}


	void ASTOpLayoutFromMesh::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Mesh);
	}


	void ASTOpLayoutFromMesh::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::LayoutFromMeshArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.LayoutIndex = LayoutIndex;
			if (Mesh) args.Mesh = Mesh->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}

	}


	void ASTOpLayoutFromMesh::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache)
	{
		// This shouldn't happen for this operation because it is always in a branch of layout operations that is not the main one.
		check(false);
	}


	namespace
	{

		/** Handle the optimization of a ASTOpLayoutFromMesh operation by moving it down its subtree. */
		class FSinkLayoutFromMesh
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.
			mu::Ptr<ASTOp> Apply(const ASTOpLayoutFromMesh* InRoot)
			{
				InitialRoot = InRoot;
				check(InitialRoot);

				OldToNew.Empty();

				InitialSource = InitialRoot->Mesh.child();
				mu::Ptr<ASTOp> NewSource = Visit(InitialSource, InitialRoot);

				// If there is any change, it is the new root.
				if (NewSource != InitialSource)
				{
					return NewSource;
				}

				return nullptr;
			}


		protected:

			const class ASTOpLayoutFromMesh* InitialRoot = nullptr;
			Ptr<ASTOp> InitialSource;
			TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

			mu::Ptr<ASTOp> Visit(const mu::Ptr<ASTOp>& at, const ASTOpLayoutFromMesh* CurrentSinkingOp)
			{
				if (!at) return nullptr;

				// Already visited?
				const Ptr<ASTOp>* Cached = OldToNew.Find({ at,CurrentSinkingOp });
				if (Cached)
				{
					return *Cached;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				case OP_TYPE::ME_MORPH:
				{
					// Sink, ignoring the op
					const ASTOpMeshMorph* Typed = static_cast<const ASTOpMeshMorph*>(at.get());
					newAt = Visit(Typed->Base.child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_FORMAT:
				{
					// Sink, ignoring the op
					const ASTOpMeshFormat* Typed = static_cast<const ASTOpMeshFormat*>(at.get());
					newAt = Visit(Typed->Source.child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_APPLYSHAPE:
				{
					// Sink, ignoring the op
					const ASTOpMeshApplyShape* Typed = static_cast<const ASTOpMeshApplyShape*>(at.get());
					newAt = Visit(Typed->Mesh.child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_BINDSHAPE:
				{
					// Sink, ignoring the op
					const ASTOpMeshBindShape* Typed = static_cast<const ASTOpMeshBindShape*>(at.get());
					newAt = Visit(Typed->Mesh.child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_ADDTAGS:
				{
					// Sink, ignoring the op
					const ASTOpMeshAddTags* Typed = static_cast<const ASTOpMeshAddTags*>(at.get());
					newAt = Visit(Typed->Source.child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_INTERPOLATE:
				{
					// Sink, ignoring the op
					const ASTOpFixed* Typed = static_cast<const ASTOpFixed*>(at.get());
					newAt = Visit(Typed->children[Typed->op.args.MeshInterpolate.base].child(), CurrentSinkingOp);
					break;
				}

				case OP_TYPE::ME_CONDITIONAL:
				{
					Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(at);
					NewConditional->type = OP_TYPE::LA_CONDITIONAL;
					NewConditional->yes = Visit(NewConditional->yes.child(), CurrentSinkingOp);
					NewConditional->no = Visit(NewConditional->no.child(), CurrentSinkingOp);
					newAt = NewConditional;
					break;
				}

				case OP_TYPE::ME_SWITCH:
				{
					Ptr<ASTOpSwitch> NewOp = mu::Clone<ASTOpSwitch>(at);
					NewOp->type = OP_TYPE::LA_SWITCH;
					NewOp->def = Visit(NewOp->def.child(), CurrentSinkingOp);
					for (ASTOpSwitch::FCase& c : NewOp->cases)
					{
						c.branch = Visit(c.branch.child(), CurrentSinkingOp);
					}
					newAt = NewOp;
					break;
				}

				case OP_TYPE::ME_MERGE:
				{
					const ASTOpFixed* Typed = static_cast<const ASTOpFixed*>(at.get());

					Ptr<ASTOpLayoutMerge> NewMerge = new ASTOpLayoutMerge;
					NewMerge->Base = Visit(Typed->children[Typed->op.args.MeshMerge.base].child(), CurrentSinkingOp);
					NewMerge->Added = Visit(Typed->children[Typed->op.args.MeshMerge.added].child(), CurrentSinkingOp);
					newAt = NewMerge;
					break;
				}

				default:
					if (at != InitialSource)
					{
						mu::Ptr<ASTOpLayoutFromMesh> NewOp = mu::Clone<ASTOpLayoutFromMesh>(CurrentSinkingOp);
						NewOp->Mesh = at;
						newAt = NewOp;
					}
					break;

				}

				OldToNew.Add({ at,CurrentSinkingOp }, newAt);

				return newAt;
			}

		};

	}


	mu::Ptr<ASTOp> ASTOpLayoutFromMesh::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		MUTABLE_CPUPROFILER_SCOPE(ASTOpLayoutFromMesh_Sink);

		FSinkLayoutFromMesh Sinker;
		mu::Ptr<ASTOp> at = Sinker.Apply(this);

		return at;
	}

}

