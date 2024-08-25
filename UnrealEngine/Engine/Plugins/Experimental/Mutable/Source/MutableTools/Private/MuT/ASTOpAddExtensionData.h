// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Math/NumericLimits.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


//---------------------------------------------------------------------------------------------
//! Adds a named ExtensionData to an Instance
//---------------------------------------------------------------------------------------------
class ASTOpAddExtensionData final : public ASTOp
{
public:

	ASTChild Instance;
	ASTChild ExtensionData;
	FString ExtensionDataName;

public:

	ASTOpAddExtensionData();
	ASTOpAddExtensionData(const ASTOpAddExtensionData&) = delete;
	~ASTOpAddExtensionData();

	OP_TYPE GetOpType() const override { return OP_TYPE::IN_ADDEXTENSIONDATA; }
	uint64 Hash() const override;
	void ForEachChild(const TFunctionRef<void(ASTChild&)> F) override;
	bool IsEqual(const ASTOp& OtherUntyped) const override;
	Ptr<ASTOp> Clone(MapChildFuncRef MapChild) const override;
	void Link(FProgram& Program, FLinkerOptions* Options) override;
};



}

