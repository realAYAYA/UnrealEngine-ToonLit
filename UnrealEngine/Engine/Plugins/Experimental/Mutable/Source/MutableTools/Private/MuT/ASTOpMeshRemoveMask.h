// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct PROGRAM;

	//---------------------------------------------------------------------------------------------
	//! From a source mesh, remove a list of fragments with a condition.
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshRemoveMask : public ASTOp
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
		void Link(PROGRAM& program, const FLinkerOptions*) override;
		Ptr<ASTOp> OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const override;

		// Own interface
		void AddRemove(const Ptr<ASTOp>& condition, const Ptr<ASTOp>& mask);
	};

}

