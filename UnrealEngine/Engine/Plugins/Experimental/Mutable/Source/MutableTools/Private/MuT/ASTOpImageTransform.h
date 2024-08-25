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
	class ASTOpImageTransform final : public ASTOp
	{
	public:

		ASTChild Base;
		ASTChild OffsetX;
		ASTChild OffsetY;
		ASTChild ScaleX;
		ASTChild ScaleY;
		ASTChild Rotation;

		uint16 SizeX = 0;
		uint16 SizeY = 0;

		uint16 SourceSizeX = 0;
		uint16 SourceSizeY = 0;

		EAddressMode AddressMode = EAddressMode::Wrap;
		bool bKeepAspectRatio = false;

	public:

		ASTOpImageTransform();
		ASTOpImageTransform(const ASTOpImageTransform&) = delete;
		~ASTOpImageTransform();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_TRANSFORM; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& OtherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef MapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& Program, FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
	};

}

