// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;

	class ASTOpImageNormalComposite : public ASTOp
	{
	public:
		ASTChild Base;
		ASTChild Normal;
		ECompositeImageMode Mode;
		float Power;

	public:

		ASTOpImageNormalComposite();
		ASTOpImageNormalComposite(const ASTOpImageNormalComposite&) = delete;
		~ASTOpImageNormalComposite();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_NORMALCOMPOSITE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool returnBestOption, GetImageDescContext* context) override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
	};

}

