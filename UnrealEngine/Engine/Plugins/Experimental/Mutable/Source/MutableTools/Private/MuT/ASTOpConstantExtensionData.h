// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


//---------------------------------------------------------------------------------------------
//! A constant ExtensionData
//---------------------------------------------------------------------------------------------
class ASTOpConstantExtensionData final : public ASTOp
{
public:

	//!
	ExtensionDataPtrConst Value;

public:

	// ASTOp interface
	OP_TYPE GetOpType() const override { return OP_TYPE::ED_CONSTANT; }
	void ForEachChild(const TFunctionRef<void(ASTChild&)>) override {}
	bool IsEqual(const ASTOp& OtherUntyped) const override;
	Ptr<ASTOp> Clone(MapChildFuncRef MapChild) const override;
	uint64 Hash() const override;
	void Link(FProgram& Program, FLinkerOptions*) override;
};


}

