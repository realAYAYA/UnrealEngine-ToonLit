// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomMeshTranslator.h"
#include "USDGeomXformableTranslator.h"

#if USE_USD_SDK

struct FUsdSchemaTranslationContext;

class FUsdGeomPointInstancerCreateAssetsTaskChain : public FBuildStaticMeshTaskChain
{
public:
	explicit FUsdGeomPointInstancerCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath, bool bIgnoreTopLevelTransformAndVisibility );

protected:
	virtual void SetupTasks() override;

private:
	bool bIgnoreTopLevelTransformAndVisibility = false;
};

class USDSCHEMAS_API FUsdGeomPointInstancerTranslator : public FUsdGeomXformableTranslator
{
public:
	using Super = FUsdGeomXformableTranslator;
	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent ) override;

	// Point instancers will always collapse (i.e. "be in charge of") their entire prim subtree.
	// If we have a nested point instancer situation, the parentmost point instancer will be in charge of collapsing everything
	// below it, so returning true to "CanBeCollapsed" in that case won't really help anything.
	// We only return true from CanBeCollapsed if e.g. a generic Xform prim should be able to collapse us too (like when we're
	// collapsing even simple, not-nested point instancers).
	virtual bool CollapsesChildren( ECollapsingType CollapsingType ) const override { return true; }
	virtual bool CanBeCollapsed( ECollapsingType CollapsingType ) const override { return Context->bCollapseTopLevelPointInstancers; }
};

#endif // #if USE_USD_SDK
