// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"

namespace UE::NNEHlslShaders::Internal
{
	class FHlslShaderBase : public FGlobalShader
	{
	public:

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

		FHlslShaderBase() {}
		FHlslShaderBase(const ShaderMetaType::CompiledShaderInitializerType& initType) : FGlobalShader(initType) {}
	};
	
} // UE::NNEHlslShaders::Internal