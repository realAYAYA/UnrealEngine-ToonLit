// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshApplyShape : public ASTOp
	{
	public:

		ASTChild Mesh;
		ASTChild Shape;
		bool m_reshapeSkeleton = false;
		bool m_reshapePhysicsVolumes = false;
		bool m_reshapeVertices = true;

	public:

		ASTOpMeshApplyShape();
		ASTOpMeshApplyShape(const ASTOpMeshApplyShape&) = delete;
		~ASTOpMeshApplyShape();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_APPLYSHAPE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};

}

