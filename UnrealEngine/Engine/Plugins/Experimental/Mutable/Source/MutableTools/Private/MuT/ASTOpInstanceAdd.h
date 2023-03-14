// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//! Operations to add elements to an instance
	//---------------------------------------------------------------------------------------------
	class ASTOpInstanceAdd : public ASTOp
	{
	public:

		//! Type of switch
		OP_TYPE type;

		ASTChild instance;
		ASTChild value;
		uint32_t id = 0;
		uint32_t externalId = 0;
		string name;

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
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};


}

