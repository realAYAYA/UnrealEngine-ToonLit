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

	class ASTOpImagePixelFormat final : public ASTOp
	{
	public:

		ASTChild Source;
		EImageFormat Format = EImageFormat::IF_NONE;
		EImageFormat FormatIfAlpha = EImageFormat::IF_NONE;

	public:

		ASTOpImagePixelFormat();
		ASTOpImagePixelFormat(const ASTOpImagePixelFormat&) = delete;
		~ASTOpImagePixelFormat();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_PIXELFORMAT; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool IsImagePlainConstant(FVector4f& colour) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

	};

}

