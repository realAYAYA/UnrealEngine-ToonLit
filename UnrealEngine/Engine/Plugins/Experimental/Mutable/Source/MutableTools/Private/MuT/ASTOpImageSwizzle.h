// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	class ASTOpImageSwizzle final : public ASTOp
	{
	public:

		ASTChild Sources[MUTABLE_OP_MAX_SWIZZLE_CHANNELS];

		uint8 SourceChannels[MUTABLE_OP_MAX_SWIZZLE_CHANNELS] = { 0,0,0,0 };

		EImageFormat Format = EImageFormat::IF_NONE;

	public:

		ASTOpImageSwizzle();
		ASTOpImageSwizzle(const ASTOpImageSwizzle&) = delete;
		~ASTOpImageSwizzle();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_SWIZZLE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool IsImagePlainConstant(FVector4f& colour) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

	};

}

