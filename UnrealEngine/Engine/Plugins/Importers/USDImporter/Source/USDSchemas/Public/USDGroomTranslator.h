// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK && WITH_EDITOR

#include "USDGeometryCacheTranslator.h"
#include "USDGeomXformableTranslator.h"

class USDSCHEMAS_API FUsdGroomTranslator : public FUsdGeometryCacheTranslator
{
public:
	using Super = FUsdGeometryCacheTranslator;
	using FUsdGeometryCacheTranslator::FUsdGeometryCacheTranslator;

	virtual void CreateAssets() override;

	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent) override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;

	virtual TSet<UE::FSdfPath> CollectAuxiliaryPrims() const override;

private:
	bool IsGroomPrim() const;
};

#endif // #if USE_USD_SDK