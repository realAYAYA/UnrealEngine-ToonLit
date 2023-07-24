// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;
	
	class ASTOpLayoutFromMesh : public ASTOp
	{
	public:

		ASTChild Mesh;
		uint8_t LayoutIndex = 0;

	public:

		ASTOpLayoutFromMesh();
		ASTOpLayoutFromMesh(const ASTOpLayoutFromMesh&) = delete;
		~ASTOpLayoutFromMesh();

		OP_TYPE GetOpType() const override { return OP_TYPE::LA_FROMMESH; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;
		void GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache) override;

	};

}

