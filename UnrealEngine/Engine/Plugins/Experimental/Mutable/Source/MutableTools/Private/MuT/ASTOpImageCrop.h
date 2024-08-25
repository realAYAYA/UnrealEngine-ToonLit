// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	class ASTOpImageCrop final : public ASTOp
	{
	public:

		ASTChild Source;
		UE::Math::TIntVector2<uint16> Min = UE::Math::TIntVector2<uint16>(0, 0);
		UE::Math::TIntVector2<uint16> Size = UE::Math::TIntVector2<uint16>(0, 0);

	public:

		ASTOpImageCrop();
		ASTOpImageCrop(const ASTOpImageCrop&) = delete;
		~ASTOpImageCrop();

		virtual OP_TYPE GetOpType() const override { return OP_TYPE::IM_CROP; }
		virtual uint64 Hash() const override;
		virtual bool IsEqual(const ASTOp& otherUntyped) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual void Link(FProgram& program, FLinkerOptions* Options) override;
		virtual FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		virtual Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		virtual void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		//TODO: virtual bool IsImagePlainConstant(FVector4f& colour) const override;
		virtual Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const override;
		virtual Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;

	};


}

