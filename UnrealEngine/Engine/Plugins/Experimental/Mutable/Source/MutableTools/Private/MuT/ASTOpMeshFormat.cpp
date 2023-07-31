// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshFormat.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
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

#include <memory>
#include <utility>



namespace mu
{


//-------------------------------------------------------------------------------------------------
ASTOpMeshFormat::ASTOpMeshFormat()
    : Source(this)
    , Format(this)
{
}


ASTOpMeshFormat::~ASTOpMeshFormat()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


bool ASTOpMeshFormat::IsEqual(const ASTOp& otherUntyped) const
{
    if (const ASTOpMeshFormat* other = dynamic_cast<const ASTOpMeshFormat*>(&otherUntyped) )
    {
        return Source==other->Source && Format==other->Format && Buffers==other->Buffers;
    }
    return false;
}


uint64 ASTOpMeshFormat::Hash() const
{
	uint64 res = std::hash<void*>()(Source.child().get() );
    hash_combine( res, Format.child().get() );
    return res;
}


mu::Ptr<ASTOp> ASTOpMeshFormat::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpMeshFormat> n = new ASTOpMeshFormat();
    n->Source = mapChild(Source.child());
    n->Format = mapChild(Format.child());
	n->Buffers = Buffers;
    return n;
}


void ASTOpMeshFormat::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
    f( Source );
    f( Format );
}


void ASTOpMeshFormat::Link( PROGRAM& program, const FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::MeshFormatArgs args;
        FMemory::Memzero( &args, sizeof(args) );

		args.buffers = Buffers;
        if (Source) args.source = Source->linkedAddress;
        if (Format) args.format = Format->linkedAddress;

        linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
        //program.m_code.push_back(op);
        program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
        AppendCode(program.m_byteCode,OP_TYPE::ME_FORMAT);
        AppendCode(program.m_byteCode,args);
    }

}


mu::Ptr<ASTOp> ASTOpMeshFormat::OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS& options, OPTIMIZE_SINK_CONTEXT& context) const
{
	mu::Ptr<ASTOp> at = context.MeshFormatSinker.Apply(this);
	return at;
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> Sink_MeshFormatAST::Apply(const ASTOp* root)
{
	m_root = dynamic_cast<const ASTOpMeshFormat*>(root);
	check(m_root);

	m_oldToNew.Empty();

	m_initialSource = m_root->Source.child();
	mu::Ptr<ASTOp> newSource = Visit(m_initialSource, m_root);

	// If there is any change, it is the new root.
	if (newSource != m_initialSource)
	{
		return newSource;
	}

	return nullptr;
}


namespace
{
	mu::Ptr<const Mesh> FindBaseMeshConstant(mu::Ptr<ASTOp> at)
	{
		mu::Ptr<const Mesh> res;

		switch (at->GetOpType())
		{
		case OP_TYPE::ME_CONSTANT:
		{
			const ASTOpConstantResource* typed = dynamic_cast<const ASTOpConstantResource*>(at.get());
			res = static_cast<const Mesh*>(typed->GetValue().get());
			break;
		}

		default:
			check(false);
		}

		check(res);

		return res;
	}


	// Make a mesh format suitable to morph a particular other format.
	MeshPtr MakeMorphTargetFormat(MeshPtrConst pTargetFormat)
	{
		MUTABLE_CPUPROFILER_SCOPE(MakeMorphTargetFormat);

		// Make a morph format by adding all the vertex channels from the base into a single
		// vertex buffer, adding the vertex index channel

		int offset = 0;
		int numChannels = 0;
		TArray<MESH_BUFFER_SEMANTIC> semantics;
		TArray<int> semanticIndices;
		TArray<MESH_BUFFER_FORMAT> formats;
		TArray<int> components;
		TArray<int> offsets;

		// Add the vertex index channel
		semantics.Add(MBS_VERTEXINDEX);
		semanticIndices.Add(0);
		formats.Add(MBF_UINT32);
		components.Add(1);
		offsets.Add(offset);
		offset += components[numChannels] * GetMeshFormatData(formats[numChannels]).m_size;
		numChannels++;

		// Add the vertex channels from the new format
		for (int vb = 0; vb < pTargetFormat->GetVertexBuffers().GetBufferCount(); ++vb)
		{
			for (int c = 0; c < pTargetFormat->GetVertexBuffers().GetBufferChannelCount(vb); ++c)
			{
				// Channel info
				MESH_BUFFER_SEMANTIC semantic;
				int semanticIndex;
				MESH_BUFFER_FORMAT format;
				int component;
				pTargetFormat->GetVertexBuffers().GetChannel
				(vb, c, &semantic, &semanticIndex, &format, &component, nullptr);

				// TODO: Filter useless semantics for morphing.
				semantics.Add(semantic);
				semanticIndices.Add(semanticIndex);
				formats.Add(format);
				components.Add(component);
				offsets.Add(offset);
				offset += components[numChannels]
					* GetMeshFormatData(formats[numChannels]).m_size;
				numChannels++;
			}
		}


		MeshPtr pTargetMorphFormat = new Mesh;
		pTargetMorphFormat->GetVertexBuffers().SetBufferCount(1);
		if (numChannels > 0)
		{
			pTargetMorphFormat->GetVertexBuffers().SetBuffer(0, offset,
				numChannels,
				&semantics[0],
				&semanticIndices[0],
				&formats[0],
				&components[0],
				&offsets[0]);
		}

		return pTargetMorphFormat;
	}

}


mu::Ptr<ASTOp> Sink_MeshFormatAST::Visit(const mu::Ptr<ASTOp>& at, const ASTOpMeshFormat* currentFormatOp)
{
	if (!at) return nullptr;

	// Newly created?
	if (m_newOps.Find(at) != INDEX_NONE)
	{
		return at;
	}

	// Already visited?
	auto cacheIt = m_oldToNew.Find(at);
	if (cacheIt)
	{
		return *cacheIt;
	}

	mu::Ptr<ASTOp> newAt = at;
	switch (at->GetOpType())
	{

	case OP_TYPE::ME_APPLYLAYOUT:
	{
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshApplyLayout.mesh, Visit(newOp->children[newOp->op.args.MeshApplyLayout.mesh].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_SETSKELETON:
	{
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshSetSkeleton.source, Visit(newOp->children[newOp->op.args.MeshSetSkeleton.source].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_CLIPMORPHPLANE:
	{
		auto newOp = mu::Clone<ASTOpMeshClipMorphPlane>(at);
		newOp->source = Visit(newOp->source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_MORPH2:
	{
		// Move the format down the base of the morph
		Ptr<ASTOpMeshMorph> newOp = mu::Clone<ASTOpMeshMorph>(at);
		newOp->Base = Visit(newOp->Base.child(), currentFormatOp);

		// Reformat the morph targets to match the new format.
		// \TODO: Cache pTargetMorphFormat? motaop?
		MeshPtrConst pTargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
		MeshPtrConst pTargetMorphFormat = MakeMorphTargetFormat(pTargetFormat);

		mu::Ptr<ASTOpConstantResource> motaop = new ASTOpConstantResource();
		motaop->type = OP_TYPE::ME_CONSTANT;
		motaop->SetValue(pTargetMorphFormat, false /* useDiskCache */);
		auto targetMorphFormatAt = motaop;

		for (int32 t=0; t<newOp->Targets.Num(); ++t)
		{
			if (newOp->Targets[t])
			{
				mu::Ptr<ASTOpMeshFormat> newFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
				newFormat->Buffers =
					OP::MeshFormatArgs::BT_VERTEX
					| OP::MeshFormatArgs::BT_IGNORE_MISSING;
				newFormat->Format = targetMorphFormatAt;

				newOp->Targets[t] = Visit(newOp->Targets[t].child(), newFormat.get());
			}
		}

		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_MERGE:
	{
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshMerge.base, Visit(newOp->children[newOp->op.args.MeshMerge.base].child(), currentFormatOp));
		newOp->SetChild(newOp->op.args.MeshMerge.added, Visit(newOp->children[newOp->op.args.MeshMerge.added].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_INTERPOLATE:
	{
		// Move the format down the base of the morph
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshInterpolate.base, Visit(newOp->children[newOp->op.args.MeshInterpolate.base].child(), currentFormatOp));

		// Reformat the morph targets to match the new format.
		// \TODO: Cache pTargetMorphFormat? motaop?
		MeshPtrConst pTargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
		MeshPtrConst pTargetMorphFormat = MakeMorphTargetFormat(pTargetFormat);

		mu::Ptr<ASTOpConstantResource> motaop = new ASTOpConstantResource();
		motaop->type = OP_TYPE::ME_CONSTANT;
		motaop->SetValue(pTargetMorphFormat, false /* useDiskCache */);
		auto targetMorphFormatAt = motaop;

		for (int t = 0; t < MUTABLE_OP_MAX_INTERPOLATE_COUNT - 1; ++t)
		{
			if (newOp->children[newOp->op.args.MeshInterpolate.targets[t]])
			{
				mu::Ptr<ASTOpMeshFormat> newFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
				newFormat->Buffers =
					OP::MeshFormatArgs::BT_VERTEX
					| OP::MeshFormatArgs::BT_IGNORE_MISSING;
				newFormat->Format = targetMorphFormatAt;

				newOp->SetChild(newOp->op.args.MeshInterpolate.targets[t], Visit(newOp->children[newOp->op.args.MeshInterpolate.targets[t]].child(), newFormat.get()));
			}
		}

		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_REMOVEMASK:
	{
		auto newOp = mu::Clone<ASTOpMeshRemoveMask>(at);
		newOp->source = Visit(newOp->source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_CONDITIONAL:
	{
		auto newOp = mu::Clone<ASTOpConditional>(at);
		newOp->yes = Visit(newOp->yes.child(), currentFormatOp);
		newOp->no = Visit(newOp->no.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_SWITCH:
	{
		auto newOp = mu::Clone<ASTOpSwitch>(at);
		newOp->def = Visit(newOp->def.child(), currentFormatOp);
		for (auto& c : newOp->cases)
		{
			c.branch = Visit(c.branch.child(), currentFormatOp);
		}
		newAt = newOp;
		break;
	}

	// This cannot be sunk since the result is different. Since the clipping is now correctly
	// generated at the end of the chain when really necessary, this wrong optimisation is no 
	// longer needed.
	//case OP_TYPE::ME_CLIPMORPHPLANE:
//         {
//             // We move the mask creation down the source
//             auto typedAt = dynamic_cast<const ASTOpMeshClipMorphPlane*>(at.get());
//             newAt = Visit(typedAt->source.child());
//             break;
//         }

	case OP_TYPE::ME_FORMAT:
		// TODO: The child format can be removed. 
		// Unless channels are removed and re-added, which would change their content?
		break;

	case OP_TYPE::ME_DIFFERENCE:
	case OP_TYPE::ME_SUBTRACT:

	default:
		if (at != m_initialSource)
		{
			mu::Ptr<ASTOpMeshFormat> newOp = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
			newOp->Source = at;
			newAt = newOp;
		}
		break;

	}

	m_oldToNew.Add(at, newAt);

	return newAt;
}

}
