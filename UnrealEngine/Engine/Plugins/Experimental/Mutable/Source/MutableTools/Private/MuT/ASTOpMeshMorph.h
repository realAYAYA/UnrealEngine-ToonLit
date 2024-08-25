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
	//! From a source mesh, remove a list of fragments with a condition.
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshMorph final : public ASTOp
	{
	public:

		//! Factor selecting what morphs to apply and with what weight.
		ASTChild Factor;

		//! Base mesh to morph.
		ASTChild Base;

		//! Target to apply on the base depending on the factor
		ASTChild Target;

	public:

		ASTOpMeshMorph();
		ASTOpMeshMorph(const ASTOpMeshMorph&) = delete;
		~ASTOpMeshMorph() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_MORPH; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(FProgram& program, FLinkerOptions*) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;

	};

}

