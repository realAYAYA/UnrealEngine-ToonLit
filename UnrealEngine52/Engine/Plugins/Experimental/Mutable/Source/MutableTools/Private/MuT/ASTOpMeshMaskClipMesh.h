// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshMaskClipMesh : public ASTOp
	{
	public:

		ASTChild source;
		ASTChild clip;

	public:

		ASTOpMeshMaskClipMesh();
		ASTOpMeshMaskClipMesh(const ASTOpMeshMaskClipMesh&) = delete;
		~ASTOpMeshMaskClipMesh();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_MASKCLIPMESH; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;

	};

}

