// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDSchemaTranslator.h"

#include "Misc/Optional.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class USceneComponent;

PXR_NAMESPACE_OPEN_SCOPE
	class UsdGeomXformable;
PXR_NAMESPACE_CLOSE_SCOPE

class USDSCHEMAS_API FUsdGeomXformableTranslator : public FUsdSchemaTranslator
{
public:
	using FUsdSchemaTranslator::FUsdSchemaTranslator;

	explicit FUsdGeomXformableTranslator( TSubclassOf< USceneComponent > InComponentTypeOverride, TSharedRef< FUsdSchemaTranslationContext > InContext, const UE::FUsdTyped& InSchema );

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent );

	virtual bool CollapsesChildren( ECollapsingType CollapsingType ) const override;
	virtual bool CanBeCollapsed( ECollapsingType CollapsingType ) const override;

	// If the optional parameters are not set, we'll figure them out automatically.
	USceneComponent* CreateComponentsEx( TOptional< TSubclassOf< USceneComponent > > ComponentType, TOptional< bool > bNeedsActor );

private:
	TOptional< TSubclassOf< USceneComponent > > ComponentTypeOverride;
};

#endif // #if USE_USD_SDK
