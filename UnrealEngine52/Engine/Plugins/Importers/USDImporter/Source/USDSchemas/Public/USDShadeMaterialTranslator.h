// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDSchemaTranslator.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class USDSCHEMAS_API FUsdShadeMaterialTranslator : public FUsdSchemaTranslator
{
public:
	using FUsdSchemaTranslator::FUsdSchemaTranslator;

	virtual void CreateAssets() override;

	virtual bool CollapsesChildren( ECollapsingType CollapsingType ) const override;
	virtual bool CanBeCollapsed( ECollapsingType CollapsingType ) const override;
};

#endif // #if USE_USD_SDK
