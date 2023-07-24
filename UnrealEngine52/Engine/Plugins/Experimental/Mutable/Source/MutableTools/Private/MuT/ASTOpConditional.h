// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;

	//---------------------------------------------------------------------------------------------
	//! Conditional operation.
	//---------------------------------------------------------------------------------------------
	class ASTOpConditional : public ASTOp
	{
	public:

		//! Type of switch
		OP_TYPE type;

		//! Boolean expression
		ASTChild condition;

		//! Branches
		ASTChild yes;
		ASTChild no;

	public:

		ASTOpConditional();
		ASTOpConditional(const ASTOpConditional&) = delete;
		~ASTOpConditional() override;

		OP_TYPE GetOpType() const override { return type; }

		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		uint64 Hash() const override;
		void Assert() override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)> f) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool returnBestOption, class FGetImageDescContext* context) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		void GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache) override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions&) const override;
		bool GetNonBlackRect(FImageRect& maskUsage) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
	};


}

