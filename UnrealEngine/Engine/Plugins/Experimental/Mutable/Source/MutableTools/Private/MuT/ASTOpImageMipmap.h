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

	class ASTOpImageMipmap final : public ASTOp
	{
	public:

		ASTChild Source;

		uint8 Levels = 0;

		//! Number of mipmaps that can be generated for a single layout block.
		uint8 BlockLevels = 0;

		//! This is true if this operation is supposed to build only the tail mipmaps.
		//! It is used during the code optimisation phase, and to validate the code.
		bool bOnlyTail = false;

		/** If this is enabled, at optimize time, the mip operation will not be split in top and bottom mip (for compose tails). */
		bool bPreventSplitTail = false;

		//! Mipmap generation settings. 
		float SharpenFactor = 0.0f;
		EAddressMode AddressMode = EAddressMode::None;
		EMipmapFilterType FilterType = EMipmapFilterType::MFT_Unfiltered;
		bool DitherMipmapAlpha = false;

	public:

		ASTOpImageMipmap();
		ASTOpImageMipmap(const ASTOpImageMipmap&) = delete;
		~ASTOpImageMipmap();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_MIPMAP; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		mu::Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool IsImagePlainConstant(FVector4f& colour) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

	};

}

