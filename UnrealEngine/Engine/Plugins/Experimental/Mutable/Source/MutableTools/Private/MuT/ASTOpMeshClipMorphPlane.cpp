// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshClipMorphPlane.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshClipMorphPlane::ASTOpMeshClipMorphPlane()
		: source(this)
	{
	}


	ASTOpMeshClipMorphPlane::~ASTOpMeshClipMorphPlane()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshClipMorphPlane::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpMeshClipMorphPlane*>(&otherUntyped))
		{
			return source == other->source &&
				morphShape == other->morphShape &&
				selectionShape == other->selectionShape &&
				vertexSelectionBone == other->vertexSelectionBone &&
				vertexSelectionType == other->vertexSelectionType &&
				dist == other->dist &&
				factor == other->factor;
		}
		return false;
	}


	uint64 ASTOpMeshClipMorphPlane::Hash() const
	{
		uint64 res = std::hash<void*>()(source.child().get());
		hash_combine(res, factor);
		hash_combine(res, dist);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshClipMorphPlane::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshClipMorphPlane> n = new ASTOpMeshClipMorphPlane();
		n->source = mapChild(source.child());
		n->morphShape = morphShape;
		n->selectionShape = selectionShape;
		n->vertexSelectionBone = vertexSelectionBone;
		n->vertexSelectionType = vertexSelectionType;
		n->dist = dist;
		n->factor = factor;
		return n;
	}


	void ASTOpMeshClipMorphPlane::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
	}


	void ASTOpMeshClipMorphPlane::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshClipMorphPlaneArgs args;
			memset(&args, 0, sizeof(args));

			if (source) args.source = source->linkedAddress;

			args.morphShape = (OP::ADDRESS)program.m_constantShapes.Num();
			program.m_constantShapes.Add(morphShape);

			args.vertexSelectionType = (uint8_t)vertexSelectionType;
			if (vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY)
			{
				args.vertexSelectionShapeOrBone = program.AddConstant(vertexSelectionBone);
			}
			else if (vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_SHAPE)
			{
				args.vertexSelectionShapeOrBone = program.AddConstant(selectionShape);
			}

			args.dist = dist;
			args.factor = factor;
			args.maxBoneRadius = vertexSelectionBoneMaxRadius;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			//program.m_code.push_back(op);
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ME_CLIPMORPHPLANE);
			AppendCode(program.m_byteCode, args);
		}

	}

}
