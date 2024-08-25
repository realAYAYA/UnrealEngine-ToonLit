// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;


	//---------------------------------------------------------------------------------------------
	//! Parameter operation.
	//---------------------------------------------------------------------------------------------
	class ASTOpParameter final : public ASTOp
	{
	public:

		//! Type of parameter
		OP_TYPE type;

		//!
		FParameterDesc parameter;

		//** Ranges adding dimensions to this parameter. */
		TArray<FRangeData> ranges;

		/** Index of the parameter in the program parameter list. Generated ar link time. */
		int32 LinkedParameterIndex = -1;

	public:

		~ASTOpParameter() override;

		OP_TYPE GetOpType() const override { return type; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Assert() override;
		void Link(FProgram& program, FLinkerOptions*) override;
		int EvaluateInt(ASTOpList& facts, bool& unknown) const override;
		FBoolEvalResult EvaluateBool(ASTOpList& /*facts*/, FEvaluateBoolCache* = nullptr) const override;
		FImageDesc GetImageDesc(bool, FGetImageDescContext*) const override;

	};

}

