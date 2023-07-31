// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomMeshTranslator.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdSkelRoot;
PXR_NAMESPACE_CLOSE_SCOPE

struct FUsdSchemaTranslationContext;

class USDSCHEMAS_API FUsdSkelRootTranslator : public FUsdGeomXformableTranslator
{
	using Super = FUsdGeomXformableTranslator;

public:
	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent ) override;

	virtual bool CollapsesChildren( ECollapsingType CollapsingType ) const override { return true; }
	virtual bool CanBeCollapsed( ECollapsingType CollapsingType ) const override { return false; }
};

#endif //#if USE_USD_SDK
