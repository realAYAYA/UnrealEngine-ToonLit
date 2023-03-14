// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDShadeMaterialTranslator.h"

#if USE_USD_SDK && WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class FMdlUsdShadeMaterialTranslator : public FUsdShadeMaterialTranslator
{
	using Super = FUsdShadeMaterialTranslator;

public:
	static FName MdlRenderContext;

public:
	using FUsdShadeMaterialTranslator::FUsdShadeMaterialTranslator;

	virtual void CreateAssets() override;
	
};

#endif // #if USE_USD_SDK && WITH_EDITOR
