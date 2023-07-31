// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpScalarCurve : public ASTOp
	{
	public:

		ASTChild time;

		//!
		Curve curve;

	public:

		ASTOpScalarCurve();
		ASTOpScalarCurve(const ASTOpScalarCurve&) = delete;
		~ASTOpScalarCurve() override;

		OP_TYPE GetOpType() const override { return OP_TYPE::PR_CONSTANT; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};


}

