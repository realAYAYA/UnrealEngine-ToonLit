// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDSchemaTranslator.h"

#if USE_USD_SDK

class USDSCHEMAS_API FUsdShadeMaterialTranslator : public FUsdSchemaTranslator
{
public:
	using FUsdSchemaTranslator::FUsdSchemaTranslator;

	virtual void CreateAssets() override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;

	virtual TSet<UE::FSdfPath> CollectAuxiliaryPrims() const override;

protected:
	virtual void PostImportMaterial(const FString& PrefixedMaterialHash, UMaterialInterface* ImportedMaterial);
};

#endif	  // #if USE_USD_SDK
