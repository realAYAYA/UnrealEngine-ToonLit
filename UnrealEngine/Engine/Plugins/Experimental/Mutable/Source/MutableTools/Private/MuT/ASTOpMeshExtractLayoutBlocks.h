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
	//! Variable sized mesh block extract operation.
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshExtractLayoutBlocks : public ASTOp
	{
	public:

		//! Source mesh to extract block from.
		ASTChild source;

		//! Layout to use to select the blocks.
		uint16 layout = 0;

		//! Blocks
		TArray<uint32> blocks;

	public:

		ASTOpMeshExtractLayoutBlocks();
		ASTOpMeshExtractLayoutBlocks(const ASTOpMeshExtractLayoutBlocks&) = delete;
		~ASTOpMeshExtractLayoutBlocks() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_EXTRACTLAYOUTBLOCK; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Assert() override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;

	};


}

