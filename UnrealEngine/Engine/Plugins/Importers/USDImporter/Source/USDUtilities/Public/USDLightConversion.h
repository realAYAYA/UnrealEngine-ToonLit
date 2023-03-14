// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK

#include "CoreMinimal.h"

#include "USDConversionUtils.h"
#include "UsdWrappers/ForwardDeclarations.h"

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
class UsdLuxDiskLight;
class UsdLuxDistantLight;
class UsdLuxDomeLight;
class UsdLuxLightAPI;
class UsdLuxRectLight;
class UsdLuxShapingAPI;
class UsdLuxSphereLight;

class UsdPrim;
class UsdStage;
template< typename T > class TfRefPtr;

using UsdStageRefPtr = TfRefPtr< UsdStage >;
PXR_NAMESPACE_CLOSE_SCOPE

class UDirectionalLightComponent;
class ULightComponentBase;
class UPointLightComponent;
class URectLightComponent;
class USkyLightComponent;
class USpotLightComponent;
class UUsdAssetCache;
enum class ELightUnits : uint8;
struct FUsdStageInfo;

/**
 * Converts UsdLux light attributes to the corresponding ULightComponent.
 *
 * Corresponding UsdLux light schema to Unreal component:
 *
 *	UsdLuxLightAPI		->	ULightComponent
 *	UsdLuxDistantLight	->	UDirectionalLightComponent
 *	UsdLuxRectLight		->	URectLightComponent
 *	UsdLuxDiskLight		->	URectLightComponent
 *	UsdLuxSphereLight	->	UPointLightComponent
 *	UsdLuxDomeLight 	->	USkyLightComponent
 *	UsdLuxShapingAPI 	->	USpotLightComponent
 */
namespace UsdToUnreal
{
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertLight( const pxr::UsdLuxLightAPI& LightAPI, ULightComponentBase& LightComponentBase, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertDistantLight( const pxr::UsdLuxDistantLight& DistantLight, UDirectionalLightComponent& LightComponent, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertRectLight( const FUsdStageInfo& StageInfo, const pxr::UsdLuxRectLight& RectLight, URectLightComponent& LightComponent, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertDiskLight( const FUsdStageInfo& StageInfo, const pxr::UsdLuxDiskLight& DiskLight, URectLightComponent& LightComponent, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertSphereLight( const FUsdStageInfo& StageInfo, const pxr::UsdLuxSphereLight& SphereLight, UPointLightComponent& LightComponent, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertDomeLight( const FUsdStageInfo& StageInfo, const pxr::UsdLuxDomeLight& DomeLight, USkyLightComponent& LightComponent, UUsdAssetCache* TexturesCache, double TimeCode );
	UE_DEPRECATED( 5.0, "Prefer the overload that receives a pxr::UsdPrim" )
	USDUTILITIES_API bool ConvertLuxShapingAPI( const FUsdStageInfo& StageInfo, const pxr::UsdLuxShapingAPI& ShapingAPI, USpotLightComponent& LightComponent, double TimeCode );

	USDUTILITIES_API bool ConvertLight( const pxr::UsdPrim& Prim, ULightComponentBase& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertDistantLight( const pxr::UsdPrim& Prim, UDirectionalLightComponent& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertRectLight( const pxr::UsdPrim& Prim, URectLightComponent& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertDiskLight( const pxr::UsdPrim& Prim, URectLightComponent& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertSphereLight( const pxr::UsdPrim& Prim, UPointLightComponent& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertDomeLight( const pxr::UsdPrim& Prim, USkyLightComponent& LightComponent, UUsdAssetCache* TexturesCache );
	USDUTILITIES_API bool ConvertLuxShapingAPI( const pxr::UsdPrim& Prim, USpotLightComponent& LightComponent, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );

	// These are separately exposed so that they can be reused when reading data into MovieScene tracks.
	// The other attribute conversions are mostly trivial like a single call to UsdToUnreal::ConvertDistance.
	// Intensity return values are always in Lumen
	USDUTILITIES_API float ConvertLightIntensityAttr( float UsdIntensity, float UsdExposure );
	USDUTILITIES_API float ConvertDistantLightIntensityAttr( float UsdIntensity, float UsdExposure );
	USDUTILITIES_API float ConvertRectLightIntensityAttr( float UsdIntensity, float UsdExposure, float UsdWidth, float UsdHeight, const FUsdStageInfo& StageInfo );
	USDUTILITIES_API float ConvertDiskLightIntensityAttr( float UsdIntensity, float UsdExposure, float UsdRadius, const FUsdStageInfo& StageInfo );
	USDUTILITIES_API float ConvertSphereLightIntensityAttr( float UsdIntensity, float UsdExposure, float UsdRadius, const FUsdStageInfo& StageInfo );
	USDUTILITIES_API float ConvertLuxShapingAPIIntensityAttr( float UsdIntensity, float UsdExposure, float UsdRadius, float UsdConeAngle, float UsdConeSoftness, const FUsdStageInfo& StageInfo );
	USDUTILITIES_API float ConvertConeAngleSoftnessAttr( float UsdConeAngle, float UsdConeSoftness, float& OutInnerConeAngle );
}

namespace UnrealToUsd
{
	USDUTILITIES_API bool ConvertLightComponent( const ULightComponentBase& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertDirectionalLightComponent( const UDirectionalLightComponent& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertRectLightComponent( const URectLightComponent& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertPointLightComponent( const UPointLightComponent& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertSkyLightComponent( const USkyLightComponent& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API bool ConvertSpotLightComponent( const USpotLightComponent& LightComponent, pxr::UsdPrim& Prim, double UsdTimeCode = UsdUtils::GetDefaultTimeCode() );

	USDUTILITIES_API float ConvertLightIntensityProperty( float Intensity );
	USDUTILITIES_API float ConvertRectLightIntensityProperty( float Intensity, float Width, float Height, const FUsdStageInfo& StageInfo, ELightUnits SourceUnits );
	USDUTILITIES_API float ConvertRectLightIntensityProperty( float Intensity, float Radius, const FUsdStageInfo& StageInfo, ELightUnits SourceUnits );  // For RectLight -> UsdLuxDiskLight
	USDUTILITIES_API float ConvertPointLightIntensityProperty( float Intensity, float SourceRadius, const FUsdStageInfo& StageInfo, ELightUnits SourceUnits );
	USDUTILITIES_API float ConvertSpotLightIntensityProperty( float Intensity, float OuterConeAngle, float InnerConeAngle, float SourceRadius, const FUsdStageInfo& StageInfo, ELightUnits SourceUnits );
	USDUTILITIES_API float ConvertOuterConeAngleProperty( float OuterConeAngle ); // Returns ConeAngleAttr() in USD
	USDUTILITIES_API float ConvertInnerConeAngleProperty( float InnerConeAngle, float OuterConeAngle ); // Returns ConeSoftnessAttr() in USD
}

#endif // #if USE_USD_SDK
