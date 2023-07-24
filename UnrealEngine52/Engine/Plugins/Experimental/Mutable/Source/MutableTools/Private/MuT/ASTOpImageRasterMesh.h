// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	class ASTOpImageRasterMesh : public ASTOp
	{
	public:

		ASTChild mesh;
		ASTChild image;
		ASTChild angleFadeProperties;
		ASTChild mask;
		ASTChild projector;

		int32 blockIndex;
		uint16 sizeX, sizeY;
		uint8 bIsRGBFadingEnabled : 1;
		uint8 bIsAlphaFadingEnabled : 1;

	public:

		ASTOpImageRasterMesh();
		ASTOpImageRasterMesh(const ASTOpImageRasterMesh&) = delete;
		~ASTOpImageRasterMesh();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_RASTERMESH; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options) const;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const;

	};

}

