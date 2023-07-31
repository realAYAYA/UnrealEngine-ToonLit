// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshRemoveMask.h"

#include "MuT/ASTOpMeshMorph.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	ASTOpMeshRemoveMask::ASTOpMeshRemoveMask()
		: source(this)
	{
	}


	//---------------------------------------------------------------------------------------------
	ASTOpMeshRemoveMask::~ASTOpMeshRemoveMask()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshRemoveMask::AddRemove(const Ptr<ASTOp>& condition, const Ptr<ASTOp>& mask)
	{
		removes.Add({ ASTChild(this,condition), ASTChild(this,mask) });
	}


	//---------------------------------------------------------------------------------------------
	bool ASTOpMeshRemoveMask::IsEqual(const ASTOp& otherUntyped) const
	{
		if (const ASTOpMeshRemoveMask* other = dynamic_cast<const ASTOpMeshRemoveMask*>(&otherUntyped))
		{
			return source == other->source && removes == other->removes;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshRemoveMask::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshRemoveMask> n = new ASTOpMeshRemoveMask();
		n->source = mapChild(source.child());
		for (const TPair<ASTChild, ASTChild>& r : removes)
		{
			n->removes.Add({ ASTChild(n,mapChild(r.Key.child())), ASTChild(n,mapChild(r.Value.child())) });
		}
		return n;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshRemoveMask::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
		for (TPair<ASTChild, ASTChild>& r : removes)
		{
			f(r.Key);
			f(r.Value);
		}
	}


	//---------------------------------------------------------------------------------------------
	uint64 ASTOpMeshRemoveMask::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(source.child().get());
		for (const TPair<ASTChild, ASTChild>& r : removes)
		{
			hash_combine(res, r.Key.child().get());
			hash_combine(res, r.Value.child().get());
		}
		return res;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshRemoveMask::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();

			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_REMOVEMASK);
			OP::ADDRESS sourceAt = source ? source->linkedAddress : 0;
			AppendCode(program.m_byteCode, sourceAt);
			AppendCode(program.m_byteCode, (uint16)removes.Num());
			for (const TPair<ASTChild, ASTChild>& b : removes)
			{
				OP::ADDRESS condition = b.Key ? b.Key->linkedAddress : 0;
				AppendCode(program.m_byteCode, condition);

				OP::ADDRESS remove = b.Value ? b.Value->linkedAddress : 0;
				AppendCode(program.m_byteCode, remove);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	namespace
	{
		class Sink_MeshRemoveMaskAST
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.

			mu::Ptr<ASTOp> Apply(const ASTOpMeshRemoveMask* root)
			{
				m_root = root;
				m_oldToNew.Empty();

				m_initialSource = root->source.child();
				mu::Ptr<ASTOp> newSource = Visit(m_initialSource);

				// If there is any change, it is the new root.
				if (newSource != m_initialSource)
				{
					return newSource;
				}

				return nullptr;
			}

		protected:

			const ASTOpMeshRemoveMask* m_root;
			mu::Ptr<ASTOp> m_initialSource;
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
				Ptr<ASTOp>* cacheIt = m_oldToNew.Find(at);
				if (cacheIt)
				{
					return *cacheIt;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				case OP_TYPE::ME_MORPH2:
				{
					mu::Ptr<ASTOpMeshMorph> newOp = mu::Clone<ASTOpMeshMorph>(at);
					newOp->Base = Visit(newOp->Base.child());
					newAt = newOp;
					break;
				}

				// disabled to avoid code explosion (or bug?) TODO
//            case OP_TYPE::ME_CONDITIONAL:
//            {
//                Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
//                newOp->yes = Visit(newOp->yes.child());
//                newOp->no = Visit(newOp->no.child());
//                newAt = newOp;
//                break;
//            }

//            case OP_TYPE::ME_SWITCH:
//            {
//                auto newOp = mu::Clone<ASTOpSwitch>(at);
//                newOp->def = Visit(newOp->def.child());
//                for( auto& c:newOp->cases )
//                {
//                    c.branch = Visit(c.branch.child());
//                }
//                newAt = newOp;
//                break;
//            }

				default:
				{
					//
					if (at != m_initialSource)
					{
						Ptr<ASTOpMeshRemoveMask> newOp = mu::Clone<ASTOpMeshRemoveMask>(m_root);
						newOp->source = at;
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
	mu::Ptr<ASTOp> ASTOpMeshRemoveMask::OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const
	{
		Sink_MeshRemoveMaskAST sinker;
		mu::Ptr<ASTOp> at = sinker.Apply(this);

		return at;
	}

}
