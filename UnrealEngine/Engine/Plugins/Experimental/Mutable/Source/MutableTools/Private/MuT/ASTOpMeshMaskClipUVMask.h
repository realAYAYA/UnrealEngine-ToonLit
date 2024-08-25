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
	class ASTOpMeshMaskClipUVMask final : public ASTOp
	{
	public:

		ASTChild Source;
		ASTChild Mask;
		uint8 LayoutIndex = 0;

	public:

		ASTOpMeshMaskClipUVMask();
		ASTOpMeshMaskClipUVMask(const ASTOpMeshMaskClipUVMask&) = delete;
		~ASTOpMeshMaskClipUVMask();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_MASKCLIPUVMASK; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;

	};

}

