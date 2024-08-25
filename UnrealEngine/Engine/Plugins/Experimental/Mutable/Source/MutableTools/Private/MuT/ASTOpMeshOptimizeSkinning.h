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
	class ASTOpMeshOptimizeSkinning final : public ASTOp
	{
	public:

		//! Mesh that will have the skinning optimized.
		ASTChild source;

	public:

		ASTOpMeshOptimizeSkinning();
		ASTOpMeshOptimizeSkinning(const ASTOpMeshOptimizeSkinning&) = delete;
		~ASTOpMeshOptimizeSkinning() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_OPTIMIZESKINNING; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(FProgram& program, FLinkerOptions*) override;
	};


}

