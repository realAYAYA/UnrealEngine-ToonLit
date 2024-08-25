// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK && WITH_EDITOR

#include "USDGeomMeshTranslator.h"

class USDSCHEMAS_API FUsdGeometryCacheTranslator : public FUsdGeomMeshTranslator
{
public:
	using Super = FUsdGeomMeshTranslator;

	using FUsdGeomMeshTranslator::FUsdGeomMeshTranslator;

	FUsdGeometryCacheTranslator(const FUsdGeometryCacheTranslator& Other) = delete;
	FUsdGeometryCacheTranslator& operator=(const FUsdGeometryCacheTranslator& Other) = delete;

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent) override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;

	virtual TSet<UE::FSdfPath> CollectAuxiliaryPrims() const override;

private:
	bool IsPotentialGeometryCacheRoot() const;
};

#endif	  // #if USE_USD_SDK
