// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDShadeMaterialTranslator.h"

#include "Custom/MaterialXUSDShadeMaterialTranslator.h"

#if USE_USD_SDK && WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class FMdlUsdShadeMaterialTranslator : public FMaterialXUsdShadeMaterialTranslator
{
	using Super = FMaterialXUsdShadeMaterialTranslator;

public:
	static FName MdlRenderContext;

public:
	using FMaterialXUsdShadeMaterialTranslator::FMaterialXUsdShadeMaterialTranslator;

	virtual void CreateAssets() override;

};

#endif // #if USE_USD_SDK && WITH_EDITOR
