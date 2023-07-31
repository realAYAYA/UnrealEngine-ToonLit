// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshClipMorphPlane : public ASTOp
	{
	public:

		ASTChild source;

		SHAPE morphShape;
		SHAPE selectionShape;
		string vertexSelectionBone;

		OP::MeshClipMorphPlaneArgs::VERTEX_SELECTION_TYPE vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_NONE;

		float dist = 0.f, factor = 0.f, vertexSelectionBoneMaxRadius = -1.f;

	public:

		ASTOpMeshClipMorphPlane();
		ASTOpMeshClipMorphPlane(const ASTOpMeshClipMorphPlane&) = delete;
		~ASTOpMeshClipMorphPlane();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_CLIPMORPHPLANE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};


}

