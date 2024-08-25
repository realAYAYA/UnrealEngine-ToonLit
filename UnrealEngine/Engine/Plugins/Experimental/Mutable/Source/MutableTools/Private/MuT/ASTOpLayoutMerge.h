// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;
	
	class ASTOpLayoutMerge final : public ASTOp
	{
	public:

		ASTChild Base;
		ASTChild Added;

	public:

		ASTOpLayoutMerge();
		ASTOpLayoutMerge(const ASTOpLayoutMerge&) = delete;
		~ASTOpLayoutMerge();

		OP_TYPE GetOpType() const override { return OP_TYPE::LA_MERGE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		void GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache) override;

	};

}

