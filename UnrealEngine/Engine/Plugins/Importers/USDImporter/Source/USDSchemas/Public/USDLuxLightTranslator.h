// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class USceneComponent;

PXR_NAMESPACE_OPEN_SCOPE
class UsdGeomXformable;
PXR_NAMESPACE_CLOSE_SCOPE

class USDSCHEMAS_API FUsdLuxLightTranslator : public FUsdGeomXformableTranslator
{
	using Super = FUsdGeomXformableTranslator;

public:

	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent );
};

#endif // #if USE_USD_SDK
