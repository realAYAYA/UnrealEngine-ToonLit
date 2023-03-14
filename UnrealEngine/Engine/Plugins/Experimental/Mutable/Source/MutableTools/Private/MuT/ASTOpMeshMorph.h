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
	class ASTOpMeshMorph : public ASTOp
	{
	public:

		//! Factor selecting what morphs to apply and with what weight.
		ASTChild Factor;

		//! Base mesh to remove from.
		ASTChild Base;

		//! Targets to apply on the base depending on the factor
		TArray<ASTChild> Targets;

	public:

		ASTOpMeshMorph();
		ASTOpMeshMorph(const ASTOpMeshMorph&) = delete;
		~ASTOpMeshMorph() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_MORPH2; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(PROGRAM& program, const FLinkerOptions*) override;
		Ptr<ASTOp> OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS&, OPTIMIZE_SINK_CONTEXT&) const override;

		// Own interface
		void AddTarget(const Ptr<ASTOp>&);
	};

}

