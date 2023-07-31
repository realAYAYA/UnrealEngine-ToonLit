// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;
template <class SCALAR> class vec4;

	class ASTOpImageMipmap : public ASTOp
	{
	public:

		ASTChild Source;

		uint8_t Levels = 0;

		//! Number of mipmaps that can be generated for a single layout block.
		uint8_t BlockLevels = 0;

		//! This is true if this operation is supposed to build only the tail mipmaps.
		//! It is used during the code optimisation phase, and to validate the code.
		bool bOnlyTail = false;

		//! Mipmap generation settings. 
		float SharpenFactor = 0.0f;
		EAddressMode AddressMode = EAddressMode::AM_NONE;
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
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS& options, OPTIMIZE_SINK_CONTEXT& context) const override;
		FImageDesc GetImageDesc(bool returnBestOption, GetImageDescContext* context) override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool IsImagePlainConstant(vec4<float>& colour) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

	};

}

