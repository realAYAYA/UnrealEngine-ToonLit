// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK
#include "UnrealUSDWrapper.h"
#include "USDMemory.h"
#include "USDStageOptions.h"

#include <string>

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class GfMatrix4d;
	class GfQuatf;
	class GfVec2f;
	class GfVec3f;
	class GfVec4f;
	class SdfPath;
	class TfToken;

	class UsdStage;
	template< typename T > class TfRefPtr;

	using UsdStageRefPtr = TfRefPtr< UsdStage >;
PXR_NAMESPACE_CLOSE_SCOPE

struct USDUTILITIES_API FUsdStageInfo
{
	EUsdUpAxis UpAxis = EUsdUpAxis::ZAxis;
	float MetersPerUnit = 0.01f;

	explicit FUsdStageInfo( const pxr::UsdStageRefPtr& Stage );
};

namespace UsdToUnreal
{
	USDUTILITIES_API FString ConvertString( const std::string& InString );
	USDUTILITIES_API FString ConvertString( std::string&& InString );
	USDUTILITIES_API FString ConvertString( const char* InString );

	USDUTILITIES_API FString ConvertPath( const pxr::SdfPath& Path );

	USDUTILITIES_API FName ConvertName( const char* InString );
	USDUTILITIES_API FName ConvertName( const std::string& InString );
	USDUTILITIES_API FName ConvertName( std::string&& InString );

	USDUTILITIES_API FString ConvertToken( const pxr::TfToken& Token );

	/** Assumes the input color is in linear space */
	USDUTILITIES_API FLinearColor ConvertColor( const pxr::GfVec3f& InValue );
	USDUTILITIES_API FLinearColor ConvertColor( const pxr::GfVec4f& InValue );

	USDUTILITIES_API FVector2D ConvertVector( const pxr::GfVec2f& InValue );
	USDUTILITIES_API FVector ConvertVector( const pxr::GfVec3f& InValue );
	USDUTILITIES_API FVector ConvertVector( const FUsdStageInfo& StageInfo, const pxr::GfVec3f& InValue );

	USDUTILITIES_API FMatrix ConvertMatrix( const pxr::GfMatrix4d& Matrix );
	USDUTILITIES_API FTransform ConvertMatrix( const FUsdStageInfo& StageInfo, const pxr::GfMatrix4d& InMatrix );

	/** Returns a distance in "UE units" (i.e. cm) */
	USDUTILITIES_API float ConvertDistance( const FUsdStageInfo& StageInfo, float InValue );
}

namespace UnrealToUsd
{
	USDUTILITIES_API TUsdStore< std::string > ConvertString( const TCHAR* InString );

	USDUTILITIES_API TUsdStore< pxr::SdfPath > ConvertPath( const TCHAR* InString );

	USDUTILITIES_API TUsdStore< std::string > ConvertName( const FName& InName );

	USDUTILITIES_API TUsdStore< pxr::TfToken > ConvertToken( const TCHAR* InString );

	/** Assumes the input color is in linear space. Returns a color in linear space */
	USDUTILITIES_API pxr::GfVec4f ConvertColor( const FLinearColor& InValue );

	/** Assumes the input color is in sRGB space. Returns a color in linear space */
	USDUTILITIES_API pxr::GfVec4f ConvertColor( const FColor& InValue );

	USDUTILITIES_API pxr::GfVec2f ConvertVector( const FVector2D& InValue );
	USDUTILITIES_API pxr::GfVec3f ConvertVector( const FVector& InValue );
	USDUTILITIES_API pxr::GfVec3f ConvertVector( const FUsdStageInfo& StageInfo, const FVector& InValue );

	USDUTILITIES_API pxr::GfMatrix4d ConvertMatrix( const FMatrix& Matrix );

	USDUTILITIES_API pxr::GfQuatf ConvertQuat( const FQuat& InValue );

	USDUTILITIES_API pxr::GfMatrix4d ConvertTransform( const FUsdStageInfo& StageInfo, const FTransform& Transform );

	/** Returns a distance in USD units (depends on metersPerUnit) */
	USDUTILITIES_API float ConvertDistance( const FUsdStageInfo& StageInfo, float InValue );
}

namespace UsdUtils
{
	USDUTILITIES_API FTransform ConvertTransformToUsdSpace( const FUsdStageInfo& StageInfo, const FTransform& TransformInUESpace );

	FTransform ConvertAxes( const bool bZUp, const FTransform Transform );
}
#endif // #if USE_USD_SDK
