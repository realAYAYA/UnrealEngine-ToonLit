// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdGeomCamera;
PXR_NAMESPACE_CLOSE_SCOPE

class USDSCHEMAS_API FUsdGeomCameraTranslator : public FUsdGeomXformableTranslator
{
public:
	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent ) override;
};

#endif // #if USE_USD_SDK
