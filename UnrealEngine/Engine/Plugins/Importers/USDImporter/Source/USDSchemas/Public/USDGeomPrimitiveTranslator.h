// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"

#include "UsdWrappers/SdfPath.h"

#if USE_USD_SDK

class USDSCHEMAS_API FUsdGeomPrimitiveTranslator : public FUsdGeomXformableTranslator
{
public:
	using Super = FUsdGeomXformableTranslator;
	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	FUsdGeomPrimitiveTranslator(const FUsdGeomPrimitiveTranslator& Other) = delete;
	FUsdGeomPrimitiveTranslator& operator=(const FUsdGeomPrimitiveTranslator& Other) = delete;

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent) override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;

	virtual TSet<UE::FSdfPath> CollectAuxiliaryPrims() const override;
};

#endif	  // #if USE_USD_SDK
