// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpConstantBool : public ASTOp
	{
	public:

		//!
		bool value;

	public:

		ASTOpConstantBool(bool value = true);

		OP_TYPE GetOpType() const override { return OP_TYPE::BO_CONSTANT; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
		BOOL_EVAL_RESULT EvaluateBool(ASTOpList& facts, EVALUATE_BOOL_CACHE* cache) const override;
	};


}

