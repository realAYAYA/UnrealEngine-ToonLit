// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDLuxLightTranslator.h"

#include "USDConversionUtils.h"
#include "USDLightConversion.h"
#include "USDTypesConversion.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/TextureCube.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/usdLux/diskLight.h"
	#include "pxr/usd/usdLux/distantLight.h"
	#include "pxr/usd/usdLux/domeLight.h"
	#include "pxr/usd/usdLux/lightAPI.h"
	#include "pxr/usd/usdLux/rectLight.h"
	#include "pxr/usd/usdLux/shapingAPI.h"
	#include "pxr/usd/usdLux/sphereLight.h"
#include "USDIncludesEnd.h"

USceneComponent* FUsdLuxLightTranslator::CreateComponents()
{
	const bool bNeedsActor = true;
	USceneComponent* LightComponent = CreateComponentsEx( {}, bNeedsActor );
	UpdateComponents( LightComponent );
	return LightComponent;
}

void FUsdLuxLightTranslator::UpdateComponents( USceneComponent* SceneComponent )
{
	Super::UpdateComponents( SceneComponent );

	ULightComponentBase* LightComponent = Cast< ULightComponentBase >( SceneComponent );

	if ( !LightComponent )
	{
		return;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim Prim = GetPrim();

	const pxr::UsdLuxLightAPI LightAPI( Prim );

	if ( !LightAPI )
	{
		return;
	}

	LightComponent->UnregisterComponent();

	UsdToUnreal::ConvertLight( Prim, *LightComponent, Context->Time );

	if ( UDirectionalLightComponent* DirectionalLightComponent = Cast< UDirectionalLightComponent >( SceneComponent ) )
	{
		UsdToUnreal::ConvertDistantLight( Prim, *DirectionalLightComponent, Context->Time );
	}
	else if ( URectLightComponent* RectLightComponent = Cast< URectLightComponent >( SceneComponent ) )
	{
		if ( pxr::UsdLuxRectLight UsdRectLight{ Prim } )
		{
			UsdToUnreal::ConvertRectLight( Prim, *RectLightComponent, Context->Time );
		}
		else if ( pxr::UsdLuxDiskLight UsdDiskLight{ Prim } )
		{
			UsdToUnreal::ConvertDiskLight( Prim, *RectLightComponent, Context->Time );
		}
	}
	else if ( UPointLightComponent* PointLightComponent = Cast< UPointLightComponent >( SceneComponent ) )
	{
		if ( USpotLightComponent* SpotLightComponent = Cast< USpotLightComponent >( SceneComponent ) )
		{
			UsdToUnreal::ConvertLuxShapingAPI( Prim, *SpotLightComponent, Context->Time );
		}

		UsdToUnreal::ConvertSphereLight( Prim, *PointLightComponent, Context->Time );
	}
	else if ( USkyLightComponent* SkyLightComponent = Cast< USkyLightComponent >( SceneComponent ) )
	{
		UsdToUnreal::ConvertDomeLight( Prim, *SkyLightComponent, Context->AssetCache.Get() );
		SkyLightComponent->Mobility = EComponentMobility::Movable; // We won't bake geometry in the sky light so it needs to be movable
	}

	if ( !LightComponent->IsRegistered() )
	{
		LightComponent->RegisterComponent();
	}
}

#endif // #if USE_USD_SDK