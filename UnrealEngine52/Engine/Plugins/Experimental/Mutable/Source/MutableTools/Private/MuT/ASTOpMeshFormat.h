// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;
	
	class ASTOpMeshFormat : public ASTOp
	{
	public:

		ASTChild Source;
		ASTChild Format;
		uint8_t Buffers = 0;

	public:

		ASTOpMeshFormat();
		ASTOpMeshFormat(const ASTOpMeshFormat&) = delete;
		~ASTOpMeshFormat();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_FORMAT; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;

	};

}

