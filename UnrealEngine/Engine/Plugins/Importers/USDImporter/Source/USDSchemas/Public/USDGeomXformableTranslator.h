// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDSchemaTranslator.h"

#include "Misc/Optional.h"

#if USE_USD_SDK

class USceneComponent;
enum class EUsdDrawMode : int32;

class USDSCHEMAS_API FUsdGeomXformableTranslator : public FUsdSchemaTranslator
{
public:
	using FUsdSchemaTranslator::FUsdSchemaTranslator;

	explicit FUsdGeomXformableTranslator(
		TSubclassOf<USceneComponent> InComponentTypeOverride,
		TSharedRef<FUsdSchemaTranslationContext> InContext,
		const UE::FUsdTyped& InSchema
	);

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents(USceneComponent* SceneComponent) override;

	virtual bool CollapsesChildren(ECollapsingType CollapsingType) const override;
	virtual bool CanBeCollapsed(ECollapsingType CollapsingType) const override;

	virtual TSet<UE::FSdfPath> CollectAuxiliaryPrims() const override;

	// If the optional parameters are not set, we'll figure them out automatically.
	USceneComponent* CreateComponentsEx(TOptional<TSubclassOf<USceneComponent>> ComponentType, TOptional<bool> bNeedsActor);

protected:
	// Creates actors, components and assets in case we have an alt draw mode like bounds or cards.
	// In theory *any* prim can have these, but we're placing these on FUsdGeomXformableTranslator as we're assuming only Xformables
	// (something drawable in the first place) can realistically use an alternative draw mode (i.e. we're not going to do much
	// placing these on a Material prim, that can't even have an Xform)
	USceneComponent* CreateAlternativeDrawModeComponents(EUsdDrawMode DrawMode);
	void UpdateAlternativeDrawModeComponents(USceneComponent* SceneComponent, EUsdDrawMode DrawMode);
	void CreateAlternativeDrawModeAssets(EUsdDrawMode DrawMode);

private:
	TOptional<TSubclassOf<USceneComponent>> ComponentTypeOverride;
};

#endif	  // #if USE_USD_SDK
