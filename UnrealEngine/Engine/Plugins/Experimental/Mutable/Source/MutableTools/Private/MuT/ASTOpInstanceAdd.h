// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


	//---------------------------------------------------------------------------------------------
	//! Operations to add elements to an instance
	//---------------------------------------------------------------------------------------------
	class ASTOpInstanceAdd final : public ASTOp
	{
	public:

		//! Type of switch
		OP_TYPE type;

		ASTChild instance;
		ASTChild value;
		uint32_t id = 0;
		uint32_t ExternalId = 0;
		int32 SharedSurfaceId = INDEX_NONE;
		FString name;

	public:

		ASTOpInstanceAdd();
		ASTOpInstanceAdd(const ASTOpInstanceAdd&) = delete;
		~ASTOpInstanceAdd();

		OP_TYPE GetOpType() const override { return type; }

		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		uint64 Hash() const override;
		void Assert() override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
	};


}

