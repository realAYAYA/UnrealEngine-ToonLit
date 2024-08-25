// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMaskClipUVMask.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshMaskClipUVMask::ASTOpMeshMaskClipUVMask()
		: Source(this)
		, Mask(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpMeshMaskClipUVMask::~ASTOpMeshMaskClipUVMask()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpMeshMaskClipUVMask::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshMaskClipUVMask* other = static_cast<const ASTOpMeshMaskClipUVMask*>(&otherUntyped);
			return Source == other->Source && Mask == other->Mask && LayoutIndex==other->LayoutIndex;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpMeshMaskClipUVMask::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, Mask.child());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMaskClipUVMask::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshMaskClipUVMask> n = new ASTOpMeshMaskClipUVMask();
		n->Source = mapChild(Source.child());
		n->Mask = mapChild(Mask.child());
		n->LayoutIndex = LayoutIndex;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshMaskClipUVMask::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
		f(Mask);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshMaskClipUVMask::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshMaskClipUVMaskArgs args;
			FMemory::Memzero(args);

			if (Source) args.Source = Source->linkedAddress;
			if (Mask) args.Mask = Mask->linkedAddress;
			args.LayoutIndex = LayoutIndex;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_MASKCLIPUVMASK);
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	namespace
	{
		class Sink_MeshMaskClipUVMaskSource
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.

			mu::Ptr<ASTOp> Apply(const ASTOpMeshMaskClipUVMask* root)
			{
				m_root = root;
				m_oldToNew.Empty();

				m_initialSource = root->Source.child();
				mu::Ptr<ASTOp> NewSource = Visit(m_initialSource);

				// If there is any change, it is the new root.
				if (NewSource != m_initialSource)
				{
					return NewSource;
				}

				return nullptr;
			}

		protected:

			const ASTOpMeshMaskClipUVMask* m_root;
			mu::Ptr<ASTOp> m_initialSource;
			TMap<mu::Ptr<ASTOp>, mu::Ptr<ASTOp>> m_oldToNew;
			TArray<mu::Ptr<ASTOp>> m_newOps;

			mu::Ptr<ASTOp> Visit(const mu::Ptr<ASTOp>& at)
			{
				if (!at) return nullptr;

				// Newly created?
				if (m_newOps.Find(at) != INDEX_NONE)
				{
					return at;
				}

				// Already visited?
				mu::Ptr<ASTOp>* cacheIt = m_oldToNew.Find(at);
				if (cacheIt)
				{
					return *cacheIt;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				// This cannot be sunk since the result is different. Since the clipping is now correctly
				// generated at the end of the chain when really necessary, this wrong optimisation is no 
				// longer needed.
				//case OP_TYPE::ME_MORPH:
		        //{
		        //    break;
		        //}

				case OP_TYPE::ME_REMOVEMASK:
				{
					// Remove this op:
					// This may lead to the mask being bigger than needed since it will include
					// faces removed by the ignored removemask, but it is ok

					// TODO: Swap instead of ignore, and implement removemask on a mask?
					const ASTOpMeshRemoveMask* typedAt = static_cast<const ASTOpMeshRemoveMask*>(at.get());
					newAt = Visit(typedAt->source.child());
					break;
				}

				case OP_TYPE::ME_ADDTAGS:
				{
					Ptr<ASTOpMeshAddTags> newOp = mu::Clone<ASTOpMeshAddTags>(at);
					newOp->Source = Visit(newOp->Source.child());
					newAt = newOp;
					break;
				}

				case OP_TYPE::ME_CONDITIONAL:
				{
					// We move the mask creation down the two paths
					// It always needs to be a clone because otherwise we could be modifying an
					// instruction that shouldn't if the parent was a ME_REMOVEMASK above and we
					// skipped the cloning for the parent.
					Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
					newOp->yes = Visit(newOp->yes.child());
					newOp->no = Visit(newOp->no.child());
					newAt = newOp;
					break;
				}

				case OP_TYPE::ME_SWITCH:
				{
					// We move the mask creation down all the paths
					Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
					newOp->def = Visit(newOp->def.child());
					for (ASTOpSwitch::FCase& c : newOp->cases)
					{
						c.branch = Visit(c.branch.child());
					}
					newAt = newOp;
					break;
				}

				// This cannot be sunk since the result is different. Since the clipping is now correctly
				// generated at the end of the chain when really necessary, this wrong optimisation is no 
				// longer needed.
				//case OP_TYPE::ME_CLIPMORPHPLANE:
	   //         {
	   //             // We move the mask creation down the Source
	   //             auto typedAt = dynamic_cast<const ASTOpMeshClipMorphPlane*>(at.get());
	   //             newAt = Visit(typedAt->Source.child());
	   //             break;
	   //         }

				default:
				{
					//
					if (at != m_initialSource)
					{
						Ptr<ASTOpMeshMaskClipUVMask> newOp = mu::Clone<ASTOpMeshMaskClipUVMask>(m_root);
						newOp->Source = at;
						newAt = newOp;
					}
					break;
				}

				}

				m_oldToNew.Add(at, newAt);

				return newAt;
			}
		};


		class Sink_MeshMaskClipUVMaskClip
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.

			mu::Ptr<ASTOp> Apply(const ASTOpMeshMaskClipUVMask* root)
			{
				m_root = root;
				m_oldToNew.Empty();

				m_initialClip = root->Mask.child();
				mu::Ptr<ASTOp> NewClip = Visit(m_initialClip);

				// If there is any change, it is the new root.
				if (NewClip != m_initialClip)
				{
					return NewClip;
				}

				return nullptr;
			}

		protected:

			const ASTOpMeshMaskClipUVMask* m_root;
			mu::Ptr<ASTOp> m_initialClip;
			TMap<mu::Ptr<ASTOp>, mu::Ptr<ASTOp>> m_oldToNew;
			TArray<mu::Ptr<ASTOp>> m_newOps;

			mu::Ptr<ASTOp> Visit(const mu::Ptr<ASTOp>& at)
			{
				if (!at) return nullptr;

				// Newly created?
				if (m_newOps.Contains(at))
				{
					return at;
				}

				// Already visited?
				mu::Ptr<ASTOp>* cacheIt = m_oldToNew.Find(at);
				if (cacheIt)
				{
					return *cacheIt;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				case OP_TYPE::ME_CONDITIONAL:
				{
					// We move the mask creation down the two paths
					// It always needs to be a clone because otherwise we could be modifying an
					// instruction that shouldn't if the parent was a ME_REMOVEMASK above and we
					// skipped the cloning for the parent.
					Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
					newOp->yes = Visit(newOp->yes.child());
					newOp->no = Visit(newOp->no.child());
					newAt = newOp;
					break;
				}

				case OP_TYPE::ME_SWITCH:
				{
					// We move the mask creation down all the paths
					Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
					newOp->def = Visit(newOp->def.child());
					for (ASTOpSwitch::FCase& c : newOp->cases)
					{
						c.branch = Visit(c.branch.child());
					}
					newAt = newOp;
					break;
				}


				default:
				{
					//
					if (at != m_initialClip)
					{
						Ptr<ASTOpMeshMaskClipUVMask> newOp = mu::Clone<ASTOpMeshMaskClipUVMask>(m_root);
						newOp->Mask = at;
						newAt = newOp;
					}
					break;
				}

				}

				m_oldToNew.Add(at, newAt);

				return newAt;
			}
		};

	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMaskClipUVMask::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		// \TODO: Add logic state to the sinkers to avoid explosion with switches in both branches and similar cases.

		// This will sink the operation down the Source
		Sink_MeshMaskClipUVMaskSource SinkerSource;
		mu::Ptr<ASTOp> at = SinkerSource.Apply(this);

		// If we didn't sink it.
		if (!at || at == this)
		{
			// This will sink the operation down the Mask child
			Sink_MeshMaskClipUVMaskClip SinkerClip;
			at = SinkerClip.Apply(this);
		}

		return at;
	}

}
 