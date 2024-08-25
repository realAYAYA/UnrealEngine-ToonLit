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
	class ASTOpMeshMorphReshape final : public ASTOp
	{
	public:

		ASTChild Morph;
		ASTChild Reshape;

	public:

		ASTOpMeshMorphReshape();
		ASTOpMeshMorphReshape(const ASTOpMeshMorphReshape&) = delete;
		~ASTOpMeshMorphReshape();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_MORPHRESHAPE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
	};


}

