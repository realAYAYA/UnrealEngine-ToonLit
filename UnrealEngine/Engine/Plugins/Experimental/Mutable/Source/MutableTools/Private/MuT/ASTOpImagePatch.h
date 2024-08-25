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

	class ASTOpImagePatch final : public ASTOp
	{
	public:

		ASTChild base;
		ASTChild patch;
		UE::Math::TIntVector2<uint16> location = UE::Math::TIntVector2<uint16>(0, 0);

	public:

		ASTOpImagePatch();
		ASTOpImagePatch(const ASTOpImagePatch&) = delete;
		~ASTOpImagePatch();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_PATCH; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		//TODO: bool IsImagePlainConstant(FVector4f& colour) const override;
	};


}

