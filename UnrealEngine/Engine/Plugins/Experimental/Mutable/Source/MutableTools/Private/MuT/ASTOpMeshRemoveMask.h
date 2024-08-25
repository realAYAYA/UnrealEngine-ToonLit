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
	class ASTOpMeshRemoveMask final : public ASTOp
	{
	public:

		//! Source mesh to remove from.
		ASTChild source;

		//! Pairs of remove candidates: condition + mesh to remove
		TArray< TPair<ASTChild, ASTChild> > removes;

	public:

		ASTOpMeshRemoveMask();
		ASTOpMeshRemoveMask(const ASTOpMeshRemoveMask&) = delete;
		~ASTOpMeshRemoveMask() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_REMOVEMASK; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(FProgram& program, FLinkerOptions*) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;

		// Own interface
		void AddRemove(const Ptr<ASTOp>& condition, const Ptr<ASTOp>& mask);
	};

}

