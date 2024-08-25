// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"

#if USE_USD_SDK

class USDSCHEMAS_API FUsdGeomCameraTranslator : public FUsdGeomXformableTranslator
{
public:
	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent) override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;
};

#endif	  // #if USE_USD_SDK
