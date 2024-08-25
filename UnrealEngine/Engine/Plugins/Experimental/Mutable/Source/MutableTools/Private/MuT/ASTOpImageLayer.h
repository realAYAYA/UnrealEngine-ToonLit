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


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpImageLayer final : public ASTOp
	{
	public:

		ASTChild base;
		ASTChild blend;
		ASTChild mask;

		/** Blend type used for the colour channels. */
		EBlendType blendType = EBlendType::BT_NONE;

		/** Blend type used for the alpha channel if any. */
		EBlendType blendTypeAlpha = EBlendType::BT_NONE;

		/** Channel to use from the source color argument to apply blendTypeAlpha, if any. */
		uint8 BlendAlphaSourceChannel = 0;

		/** See ImageLayerArgs::Flags .*/
		uint8 Flags = 0;

	public:

		ASTOpImageLayer();
		ASTOpImageLayer(const ASTOpImageLayer&) = delete;
		~ASTOpImageLayer();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_LAYER; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions*) override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext*) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const;

	};

}

