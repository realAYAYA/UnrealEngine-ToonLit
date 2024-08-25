// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"

#if USE_USD_SDK

class USceneComponent;

class USDSCHEMAS_API FUsdLuxLightTranslator : public FUsdGeomXformableTranslator
{
	using Super = FUsdGeomXformableTranslator;

public:

	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent);

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;
};

#endif	  // #if USE_USD_SDK
