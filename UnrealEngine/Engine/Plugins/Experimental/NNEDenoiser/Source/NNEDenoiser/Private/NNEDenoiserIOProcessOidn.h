// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEDenoiserIOProcessBase.h"

namespace UE::NNEDenoiser::Private
{

class FInputProcessOidn : public FInputProcessBase
{
public:
	FInputProcessOidn(FResourceMappingList InputLayout) : FInputProcessBase(InputLayout)
	{
		
	}

	virtual ~FInputProcessOidn() = default;

protected:
	virtual bool HasPreprocessInput(EResourceName TensorName, int32 FrameIdx) const override;

	virtual void PreprocessInput(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef Texture,
		EResourceName TensorName,
		int32 FrameIdx,
		FRDGTextureRef PreprocessedTexture) const override;
};

class FOutputProcessOidn : public FOutputProcessBase
{
public:
	FOutputProcessOidn(FResourceMappingList OutputLayout) : FOutputProcessBase(OutputLayout)
	{
		
	}

	virtual ~FOutputProcessOidn() = default;

protected:
	virtual bool HasPostprocessOutput(EResourceName TensorName, int32 FrameIdx) const override;

	virtual void PostprocessOutput(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef Texture,
		FRDGTextureRef PostprocessedTexture) const override;
};

} // namespace UE::NNEDenoiser::Private