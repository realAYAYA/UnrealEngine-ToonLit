// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string>

#include "CoreMinimal.h"
#include "Misc/TVariant.h"

#include "UsdWrappers/ForwardDeclarations.h"
#include "UsdWrappers/UsdAttribute.h"

namespace UE
{
	class FVtValue;
}

namespace UsdUtils
{
	/** Source: https://graphics.pixar.com/usd/docs/api/_usd__page__datatypes.html */
	enum class USDUTILITIES_API EUsdBasicDataTypes
	{
		None,

		Bool,
		Uchar,
		Int,
		Uint,
		Int64,
		Uint64,

		Half,
		Float,
		Double,

		Timecode,

		String,
		Token,
		Asset,

		Matrix2d,
		Matrix3d,
		Matrix4d,

		Quatd,
		Quatf,
		Quath,

		Double2,
		Float2,
		Half2,
		Int2,

		Double3,
		Float3,
		Half3,
		Int3,

		Double4,
		Float4,
		Half4,
		Int4,
	};

	// WARNING: Do not change the order of the types in this TVariant declaration, or it will break backwards compatibility with data that
	// was serialized before the change, due to how `operator<<( FArchive& Ar, FConvertedVtValueComponent& Component )` is implemented
	using FConvertedVtValueComponent = TVariant< bool, uint8, int32, uint32, int64, uint64, float, double, FString >;

	/** Represents a single non-array value held by a pxr::VtValue object, like a 'float', a 'TfToken' or a 'GfMatrix3d' */
	using FConvertedVtValueEntry = TArray< FConvertedVtValueComponent >;

	/** Corresponds to a value held by a pxr::VtValue converted into UE types. Should handle anything a VtValue can hold, including arrays of values */
	struct USDUTILITIES_API FConvertedVtValue
	{
		TArray<FConvertedVtValueEntry> Entries;

		EUsdBasicDataTypes SourceType = EUsdBasicDataTypes::None;
		bool bIsArrayValued = false;
		bool bIsEmpty = false;  // Helps differentiating between empty arrays and having actually no value
	};
}

USDUTILITIES_API FArchive& operator<<( FArchive& Ar, UsdUtils::FConvertedVtValue& Struct );
USDUTILITIES_API FArchive& operator<<( FArchive& Ar, UsdUtils::FConvertedVtValueComponent& Component );

namespace UsdToUnreal
{
	USDUTILITIES_API bool ConvertValue( const UE::FVtValue& InValue, UsdUtils::FConvertedVtValue& OutValue );
}

namespace UnrealToUsd
{
	USDUTILITIES_API bool ConvertValue( const UsdUtils::FConvertedVtValue& InValue, UE::FVtValue& OutValue );
}

namespace UsdUtils
{
	/** Uses USD to stringify the underlying pxr::VtValue */
	USDUTILITIES_API FString Stringify( const UE::FVtValue& Value );

	/**
	 * Uses USD to quickly fetch the underlying value of the wrapped pxr::VtValue for fundamental data types.
	 * For complex vector/array types ConvertValue must be used instead.
	 * Template implementation must be hidden on cpp as we can't expose USD implementation on the header files.
	 */
	template<typename T>
	USDUTILITIES_API TOptional<T> GetUnderlyingValue( const UE::FVtValue& InValue );

	/**
	 * Uses USD to quickly set the underlying value of the wrapped pxr::VtValue for fundamental data types.
	 * This will not allow changing the underlying type of InValue, i.e. you can only set a float if InValue
	 * already contains a float or if its otherwise empty.
	 * For complex vector/array types ConvertValue must be used instead.
	 * Template implementation must be hidden on cpp as we can't expose USD implementation on the header files.
	 */
	template<typename T>
	USDUTILITIES_API bool SetUnderlyingValue( UE::FVtValue& InValue, const T& UnderlyingValue );

	/**
	 * Returns the name of the SdfValueTypeName object for the type of Value
	 *
	 * Note that this is the "Value type token" (e.g. will be something like 'matrix4d' or 'double2', and not 'GfMatrix4d' or 'GfVec2d')
	 * Note that this may not be the correct SdfValueTypeName for the *attribute*, it's just the implied type of the value (e.g. for an
	 * attribute with SdfValueTypeName 'normal3d', it's VtValues would have underlying type 'GfVec3d', and so this function would return 'double3')
	 */
	USDUTILITIES_API FString GetImpliedTypeName( const UE::FVtValue& Value );
}

