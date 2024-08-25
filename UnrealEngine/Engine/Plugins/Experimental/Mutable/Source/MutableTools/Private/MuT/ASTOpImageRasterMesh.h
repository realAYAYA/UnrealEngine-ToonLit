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

	class ASTOpImageRasterMesh final : public ASTOp
	{
	public:

		ASTChild mesh;
		ASTChild image;
		ASTChild angleFadeProperties;
		ASTChild mask;
		ASTChild projector;

		/** If layouts are used, this can indicate a single layout block that we want to raster. */
		int32 BlockId;
		int8 LayoutIndex;

		/** Size of the image to generate by rasterization of the mesh. */
		uint16 SizeX, SizeY;

		/** Expected size of the image that we want to project. */
		uint16 SourceSizeX, SourceSizeY;

		/** Sub-rect to raster, ignoring all the rest.
		 * Only valid if any UncroppedSizeX is greater than 0. 
		 */
		uint16 CropMinX, CropMinY;
		uint16 UncroppedSizeX, UncroppedSizeY;

		uint8 bIsRGBFadingEnabled : 1;
		uint8 bIsAlphaFadingEnabled : 1;
		ESamplingMethod SamplingMethod;
		EMinFilterMethod MinFilterMethod;

	public:

		ASTOpImageRasterMesh();
		ASTOpImageRasterMesh(const ASTOpImageRasterMesh&) = delete;
		~ASTOpImageRasterMesh();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_RASTERMESH; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, FLinkerOptions* Options) override;
		FImageDesc GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;
		Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const;

	};

}

