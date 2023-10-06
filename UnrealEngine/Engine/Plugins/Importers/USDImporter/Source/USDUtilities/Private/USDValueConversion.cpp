// Copyright Epic Games, Inc. All Rights Reserved.


#include "USDValueConversion.h"

#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/VtValue.h"

#include "Containers/StringConv.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
	#include "pxr/base/vt/value.h"
	#include "pxr/usd/sdf/path.h"
	#include "pxr/usd/sdf/types.h"
	#include "pxr/usd/usd/attribute.h"
	#include "pxr/usd/usd/stage.h"
	#include "pxr/usd/usdGeom/tokens.h"
#include "USDIncludesEnd.h"
#endif // USE_USD_SDK

#if USE_USD_SDK
namespace UsdToUnrealImpl
{
	/**
	 * Allows us to use the same lambda to convert either VtArray<USDType> or USDType
	 * Func should have a signature like this:
	 * []( const USDType& Val ) -> UsdUtils::FConvertedVtValueEntry {}
	 */
	template<typename UEType, typename USDType, typename Func>
	void ConvertInner( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue, Func Function )
	{
		if ( InValue.IsArrayValued() )
		{
			OutValue.Entries.Reset( InValue.GetArraySize() );

			for ( const USDType& Val : InValue.UncheckedGet<pxr::VtArray<USDType>>() )
			{
				OutValue.Entries.Add( Function( Val ) );
			}
		}
		else
		{
			const USDType& Val = InValue.UncheckedGet<USDType>();

			OutValue.Entries = { Function( Val ) };
		}
	}

	// Bool, Uchar, Int, Uint, Int64, Uint64, Half (can cast to float), Float, Double
	template<typename UEType, typename USDType>
	void ConvertSimpleValue( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<UEType, USDType>( InValue, OutValue,
			[]( const USDType& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				return { UsdUtils::FConvertedVtValueComponent( TInPlaceType<UEType>(), static_cast< UEType >( Val ) ) };
			}
		);
	}

	// TimeCode
	template<>
	void ConvertSimpleValue<double, pxr::SdfTimeCode>( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<double, pxr::SdfTimeCode>( InValue, OutValue,
			[]( const pxr::SdfTimeCode& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				return { UsdUtils::FConvertedVtValueComponent( TInPlaceType<double>(), Val.GetValue() ) };
			}
		);
	}

	// String
	template<>
	void ConvertSimpleValue<FString, std::string>( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<FString, std::string>( InValue, OutValue,
			[]( const std::string& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				return { UsdUtils::FConvertedVtValueComponent( TInPlaceType<FString>(), UsdToUnreal::ConvertString( Val ) ) };
			}
		);
	}

	// Token
	template<>
	void ConvertSimpleValue<FString, pxr::TfToken>( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<FString, pxr::TfToken>( InValue, OutValue,
			[]( const pxr::TfToken& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				return { UsdUtils::FConvertedVtValueComponent( TInPlaceType<FString>(), UsdToUnreal::ConvertToken( Val ) ) };
			}
		);
	}

	// Asset
	template<>
	void ConvertSimpleValue<FString, pxr::SdfAssetPath>( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<FString, pxr::SdfAssetPath>( InValue, OutValue,
			[]( const pxr::SdfAssetPath& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				return { UsdUtils::FConvertedVtValueComponent( TInPlaceType<FString>(), UsdToUnreal::ConvertString( Val.GetAssetPath() ) ) };
			}
		);
	}

	// Matrix2d, Matrix3d, Matrix4d	(always double)
	template<typename USDType>
	void ConvertMatrixValue( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<double, USDType>( InValue, OutValue,
			[]( const USDType& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				const int32 NumElements = USDType::numRows * USDType::numColumns;

				UsdUtils::FConvertedVtValueEntry Entry;
				Entry.Reserve( NumElements );

				const double* MatrixArray = Val.GetArray();
				for ( int32 Index = 0; Index < NumElements; ++Index )
				{
					Entry.Emplace( TInPlaceType<double>(), MatrixArray[ Index ] );
				}

				return Entry;
			}
		);
	}

	// Quath, Quatf, Quatd
	template<typename UEType, typename USDType>
	void ConvertQuatValue( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<UEType, USDType>( InValue, OutValue,
			[]( const USDType& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				// Auto here because this is the vec of the corresponding type (e.g. Quath -> Vec3h)
				const auto& Img = Val.GetImaginary();
				double Real = Val.GetReal();

				return
				{
					UsdUtils::FConvertedVtValueComponent( TInPlaceType<UEType>(), Img[ 0 ] ),
					UsdUtils::FConvertedVtValueComponent( TInPlaceType<UEType>(), Img[ 1 ] ),
					UsdUtils::FConvertedVtValueComponent( TInPlaceType<UEType>(), Img[ 2 ] ),
					UsdUtils::FConvertedVtValueComponent( TInPlaceType<UEType>(), Real ),
				};
			}
		);
	}

	// Double2, Float2, Half2, Int2
	// Double3, Float3, Half3, Int3
	// Double4, Float4, Half4, Int4
	template<typename UEType, typename USDType>
	void ConvertVecValue( const pxr::VtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
		ConvertInner<UEType, USDType>( InValue, OutValue,
			[]( const USDType& Val ) -> UsdUtils::FConvertedVtValueEntry
			{
				UsdUtils::FConvertedVtValueEntry Entry;
				Entry.Reset( USDType::dimension );

				for ( int32 Index = 0; Index < USDType::dimension; ++Index )
				{
					Entry.Emplace( TInPlaceType<UEType>(), Val[ Index ] );
				}

				return Entry;
			}
		);
	}
}
#endif // USE_USD_SDK

namespace UsdToUnreal
{
	bool ConvertValue( const UE::FVtValue& InValue, UsdUtils::FConvertedVtValue& OutValue )
	{
#if USE_USD_SDK
		using namespace pxr;
		using namespace UsdUtils;
		using namespace UsdToUnrealImpl;

		OutValue.SourceType = UsdUtils::EUsdBasicDataTypes::None;
		OutValue.Entries.Reset();
		OutValue.bIsArrayValued = false;
		OutValue.bIsEmpty = true;

		// We consider a success returning an empty value if our input value was also empty
		if ( InValue.IsEmpty() )
		{
			return true;
		}

		OutValue.bIsEmpty = false;
		OutValue.bIsArrayValued = InValue.IsArrayValued();

		const VtValue& UsdValue = InValue.GetUsdValue();
		const TfType& UnderlyingType = UsdValue.GetType();

#pragma push_macro("CHECK_TYPE")
#define CHECK_TYPE(T) UnderlyingType.IsA<T>() || UnderlyingType.IsA<VtArray<T>>()

		if ( CHECK_TYPE(bool) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Bool;
			ConvertSimpleValue<bool, bool>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(uint8_t) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Uchar;
			ConvertSimpleValue<uint8, uint8_t>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(int32_t) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int;
			ConvertSimpleValue<int32, int32_t>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(uint32_t) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Uint;
			ConvertSimpleValue<uint32, uint32_t>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(int64_t) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int64;
			ConvertSimpleValue<int64, int64_t>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(uint64_t) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Uint64;
			ConvertSimpleValue<uint64, uint64_t>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfHalf) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Half;
			ConvertSimpleValue<float, GfHalf>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(float) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Float;
			ConvertSimpleValue<float, float>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(double) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Double;
			ConvertSimpleValue<double, double>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(SdfTimeCode) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Timecode;
			ConvertSimpleValue<double, SdfTimeCode>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(std::string) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::String;
			ConvertSimpleValue<FString, std::string>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(TfToken) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Token;
			ConvertSimpleValue<FString, TfToken>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(SdfAssetPath) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Asset;
			ConvertSimpleValue<FString, SdfAssetPath>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfMatrix2d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Matrix2d;
			ConvertMatrixValue<GfMatrix2d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfMatrix3d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Matrix3d;
			ConvertMatrixValue<GfMatrix3d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfMatrix4d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Matrix4d;
			ConvertMatrixValue<GfMatrix4d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfQuatd) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Quatd;
			ConvertQuatValue<double, GfQuatd>(UsdValue, OutValue);
		}
		else if ( CHECK_TYPE(GfQuatf) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Quatf;
			ConvertQuatValue<float, GfQuatf>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfQuath) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Quath;
			ConvertQuatValue<float, GfQuath>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec2d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Double2;
			ConvertVecValue<double, GfVec2d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec2f) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Float2;
			ConvertVecValue<float, GfVec2f>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec2h) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Half2;
			ConvertVecValue<float, GfVec2h>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec2i) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int2;
			ConvertVecValue<int32, GfVec2i>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec3d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Double3;
			ConvertVecValue<double, GfVec3d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec3f) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Float3;
			ConvertVecValue<float, GfVec3f>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec3h) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Half3;
			ConvertVecValue<float, GfVec3h>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec3i) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int3;
			ConvertVecValue<int32, GfVec3i>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec4d) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Double4;
			ConvertVecValue<double, GfVec4d>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec4f) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Float4;
			ConvertVecValue<float, GfVec4f>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec4h) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Half4;
			ConvertVecValue<float, GfVec4h>( UsdValue, OutValue );
		}
		else if ( CHECK_TYPE(GfVec4i) )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int4;
			ConvertVecValue<int32, GfVec4i>( UsdValue, OutValue );
		}
		// These types should only appear within metadata, and don't support arrays.
		// There are more of them (e.g. pxr/usd/usd/crateDataTypes.h), but these are the most common.
		// Also check pxr/usd/sdf/types.cpp for where these are defined. These are simple enums
		else if ( UnderlyingType.IsA<SdfPermission>() )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int;
			ConvertSimpleValue<int32, SdfPermission>( UsdValue, OutValue );
		}
		else if ( UnderlyingType.IsA<SdfSpecifier>() )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int;
			ConvertSimpleValue<int32, SdfSpecifier>( UsdValue, OutValue );
		}
		else if ( UnderlyingType.IsA<SdfVariability>() )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int;
			ConvertSimpleValue<int32, SdfVariability>( UsdValue, OutValue );
		}
		else if ( UnderlyingType.IsA<SdfSpecType>() )
		{
			OutValue.SourceType = EUsdBasicDataTypes::Int;
			ConvertSimpleValue<int32, SdfSpecType>( UsdValue, OutValue );
		}
		else
		{
			return false;
		}

#undef CHECK_TYPE
#pragma pop_macro("CHECK_TYPE")

		return true;
#else
		return false;
#endif // USE_USD_SDK
	}
}

#if USE_USD_SDK
namespace UnrealToUsdImpl
{
	/**
	 * Allows us to use the same lambda to convert either VtArray<USDType> or USDType
	 * Func should have a signature like this:
	 * []( const UsdUtils::FConvertedVtValueEntry& Entry ) -> USDElementType {}
	 */
	template<typename UEElementType, typename USDElementType, typename Func>
	void ConvertInner( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue, Func Function )
	{
		if ( InValue.bIsArrayValued )
		{
			pxr::VtArray<USDElementType> Array;
			Array.reserve( InValue.Entries.Num() );

			for ( const UsdUtils::FConvertedVtValueEntry& Entry : InValue.Entries )
			{
				if ( Entry.Num() > 0 )
				{
					Array.push_back( Function( Entry ) );
				}
			}

			OutValue = Array;
		}
		else if ( InValue.Entries.Num() > 0 )
		{
			const UsdUtils::FConvertedVtValueEntry& Entry = InValue.Entries[0];
			if ( Entry.Num() > 0 )
			{
				OutValue = Function( Entry );
			}
		}
	}

	template<typename UEElementType, typename USDElementType>
	void ConvertSimpleValue( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< UEElementType, USDElementType >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> USDElementType
			{
				if ( const UEElementType* Value = Entry[ 0 ].TryGet<UEElementType>() )
				{
					return static_cast< USDElementType >( *Value );
				}

				return USDElementType();
			}
		);
	}

	template<>
	void ConvertSimpleValue<FString, std::string>( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< FString, std::string >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> std::string
			{
				if ( const FString* Value = Entry[ 0 ].TryGet<FString>() )
				{
					return UnrealToUsd::ConvertString( **Value ).Get();
				}

				return std::string();
			}
		);
	}

	template<>
	void ConvertSimpleValue<FString, pxr::TfToken>( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< FString, pxr::TfToken >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> pxr::TfToken
			{
				if ( const FString* Value = Entry[ 0 ].TryGet<FString>() )
				{
					return UnrealToUsd::ConvertToken( **Value ).Get();
				}

				return pxr::TfToken();
			}
		);
	}

	template<>
	void ConvertSimpleValue<FString, pxr::SdfAssetPath>( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< FString, pxr::SdfAssetPath >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> pxr::SdfAssetPath
			{
				if ( const FString* Value = Entry[ 0 ].TryGet<FString>() )
				{
					return pxr::SdfAssetPath( UnrealToUsd::ConvertString( **Value ).Get() );
				}

				return pxr::SdfAssetPath();
			}
		);
	}

	/** We need the USDElementType parameter to do the final float to pxr::GfHalf conversions */
	template<typename UEElementType, typename USDArrayType, typename USDElementType = UEElementType>
	void ConvertCompoundValue( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< UEElementType, USDArrayType >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> USDArrayType
			{
				USDArrayType USDVal( UEElementType( 0 ) );
				USDElementType* DataPtr = USDVal.data();

				for ( int32 Index = 0; Index < Entry.Num(); ++Index )
				{
					if ( const UEElementType* IndexValue = Entry[ Index ].TryGet<UEElementType>() )
					{
						DataPtr[ Index ] = static_cast< USDElementType >( *IndexValue );
					}
				}
				return USDVal;
			}
		);

	}

	/** USD quaternions don't have the access operator defined, and the elements need to be reordered */
	template<typename UEElementType, typename USDQuatType>
	void ConvertQuatValue( const UsdUtils::FConvertedVtValue& InValue, pxr::VtValue& OutValue )
	{
		ConvertInner< UEElementType, USDQuatType >( InValue, OutValue,
			[]( const UsdUtils::FConvertedVtValueEntry& Entry ) -> USDQuatType
			{
				if ( Entry.Num() == 4 )
				{
					UEElementType QuatValues[ 4 ] = { 0, 0, 0, 1 };
					for ( int32 Index = 0; Index < 4; ++Index )
					{
						if ( const UEElementType* QuatValue = Entry[ Index ].TryGet<UEElementType>() )
						{
							QuatValues[ Index ] = *QuatValue;
						}
					}

					return USDQuatType(
						QuatValues[ 3 ],  // Real part comes first for USD
						QuatValues[ 0 ],
						QuatValues[ 1 ],
						QuatValues[ 2 ]
					);
				}

				return USDQuatType();
			}
		);
	}
}
#endif // USE_USD_SDK

namespace UnrealToUsd
{
	bool ConvertValue( const UsdUtils::FConvertedVtValue& InValue, UE::FVtValue& OutValue )
	{
#if USE_USD_SDK
		using namespace pxr;
		using namespace UsdUtils;
		using namespace UnrealToUsdImpl;

		// Always USD Allocs here because we may end up allocating arrays for these compound types,
		// and those arrays can be moved into the UE::FVtValue's pxr::VtValue and, when set in an
		// attribute, destroyed internally by USD
		FScopedUsdAllocs Allocs;

		VtValue& UsdValue = OutValue.GetUsdValue();

		switch ( InValue.SourceType )
		{
		case EUsdBasicDataTypes::Bool:
			ConvertSimpleValue<bool, bool>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Uchar:
			ConvertSimpleValue<uint8, uint8_t>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Int:
			ConvertSimpleValue<int32, int32_t>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Uint:
			ConvertSimpleValue<uint32, uint32_t>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Int64:
			ConvertSimpleValue<int64, int64_t>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Uint64:
			ConvertSimpleValue<uint64, uint64_t>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Half:
			ConvertSimpleValue<float, GfHalf>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Float:
			ConvertSimpleValue<float, float>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Double:
			ConvertSimpleValue<double, double>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Timecode:
			ConvertSimpleValue<double, SdfTimeCode>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::String:
			ConvertSimpleValue<FString, std::string>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Token:
			ConvertSimpleValue<FString, TfToken>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Asset:
			ConvertSimpleValue<FString, SdfAssetPath>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Matrix2d:
			ConvertCompoundValue<double, GfMatrix2d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Matrix3d:
			ConvertCompoundValue<double, GfMatrix3d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Matrix4d:
			ConvertCompoundValue<double, GfMatrix4d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Quatd:
			ConvertQuatValue<double, GfQuatd>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Quatf:
			ConvertQuatValue<float, GfQuatf>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Quath:
			ConvertQuatValue<float, GfQuath>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Double2:
			ConvertCompoundValue<double, GfVec2d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Float2:
			ConvertCompoundValue<float, GfVec2f>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Half2:
			ConvertCompoundValue<float, GfVec2h, GfHalf>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Int2:
			ConvertCompoundValue<int32, GfVec2i>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Double3:
			ConvertCompoundValue<double, GfVec3d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Float3:
			ConvertCompoundValue<float, GfVec3f>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Half3:
			ConvertCompoundValue<float, GfVec3h, GfHalf>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Int3:
			ConvertCompoundValue<int32, GfVec3i>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Double4:
			ConvertCompoundValue<double, GfVec4d>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Float4:
			ConvertCompoundValue<float, GfVec4f>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Half4:
			ConvertCompoundValue<float, GfVec4h, GfHalf>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::Int4:
			ConvertCompoundValue<int32, GfVec4i>( InValue, UsdValue );
			break;
		case EUsdBasicDataTypes::None:
		default:
			break;
		}

		// We consider a success returning an empty value if our input value was also empty
		return InValue.Entries.Num() == 0 || !OutValue.IsEmpty();
#else
		return false;
#endif // USE_USD_SDK
	}
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FConvertedVtValue& Struct )
{
	Ar << Struct.Entries;
	Ar << Struct.SourceType;
	Ar << Struct.bIsArrayValued;
	Ar << Struct.bIsEmpty;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FConvertedVtValueComponent& Component )
{
	if ( Ar.IsSaving() )
	{
		uint64 TypeIndex = Component.GetIndex();
		Ar << TypeIndex;

		switch ( TypeIndex )
		{
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<bool>() :
				if ( bool* Val = Component.TryGet<bool>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint8>() :
				if ( uint8* Val = Component.TryGet<uint8>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<int32>() :
				if ( int32* Val = Component.TryGet<int32>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint32>() :
				if ( uint32* Val = Component.TryGet<uint32>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<int64>() :
				if ( int64* Val = Component.TryGet<int64>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint64>() :
				if ( uint64* Val = Component.TryGet<uint64>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<float>() :
				if ( float* Val = Component.TryGet<float>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<double>() :
				if ( double* Val = Component.TryGet<double>() )
				{
					Ar << *Val;
				}
			break;
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<FString>() :
				if ( FString* Val = Component.TryGet<FString>() )
				{
					Ar << *Val;
				}
			break;
			default:
				break;
		}
	}
	else // IsLoading
	{
		uint64 TypeIndex;
		Ar << TypeIndex;

		switch ( TypeIndex )
		{
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<bool>() :
			{
				bool Val;
				Ar << Val;
				Component.Set<bool>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint8>() :
			{
				uint8 Val;
				Ar << Val;
				Component.Set<uint8>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<int32>() :
			{
				int32 Val;
				Ar << Val;
				Component.Set<int32>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint32>() :
			{
				uint32 Val;
				Ar << Val;
				Component.Set<uint32>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<int64>() :
			{
				int64 Val;
				Ar << Val;
				Component.Set<int64>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<uint64>() :
			{
				uint64 Val;
				Ar << Val;
				Component.Set<uint64>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<float>() :
			{
				float Val;
				Ar << Val;
				Component.Set<float>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<double>() :
			{
				double Val;
				Ar << Val;
				Component.Set<double>( Val );
				break;
			}
			case UsdUtils::FConvertedVtValueComponent::IndexOfType<FString>() :
			{
				FString Val;
				Ar << Val;
				Component.Set<FString>( Val );
				break;
			}
			default:
				Component.Set<bool>( false );
				break;
		}
	}

	return Ar;
}

namespace UsdUtils
{
	FString Stringify( const UE::FVtValue& Value )
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::VtValue& UsdValue = Value.GetUsdValue();
		return UsdToUnreal::ConvertString( pxr::TfStringify( UsdValue ) );
#else
		return FString();
#endif // USE_USD_SDK
	}

	template<typename T>
	TOptional<T> GetUnderlyingValue( const UE::FVtValue& InValue )
	{
#if USE_USD_SDK
		const pxr::VtValue& UsdValue = InValue.GetUsdValue();
		if ( UsdValue.IsHolding<T>() )
		{
			return { UsdValue.UncheckedGet<T>() };
		}
#endif // USE_USD_SDK

		return {};
	}
    template USDUTILITIES_API TOptional<bool> GetUnderlyingValue<bool>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<float> GetUnderlyingValue<float>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<uint8_t> GetUnderlyingValue<uint8_t>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<int32_t> GetUnderlyingValue<int32_t>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<uint32_t> GetUnderlyingValue<uint32_t>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<int64_t> GetUnderlyingValue<int64_t>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<uint64_t> GetUnderlyingValue<uint64_t>( const UE::FVtValue& InValue );
	template USDUTILITIES_API TOptional<std::string> GetUnderlyingValue<std::string>( const UE::FVtValue& InValue );

	template<typename T>
	bool SetUnderlyingValue( UE::FVtValue& InValue, const T& UnderlyingValue )
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		pxr::VtValue& UsdValue = InValue.GetUsdValue();
		if ( UsdValue.IsHolding<T>() || UsdValue.IsEmpty() )
		{
			UsdValue = UnderlyingValue;
			return true;
		}
#endif // USE_USD_SDK

		return false;
	}
    template bool USDUTILITIES_API SetUnderlyingValue( UE::FVtValue& InValue, const bool& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue( UE::FVtValue& InValue, const float& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue<uint8_t>( UE::FVtValue& InValue, const uint8_t& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue<int32_t>( UE::FVtValue& InValue, const int32_t& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue<uint32_t>( UE::FVtValue& InValue, const uint32_t& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue<int64_t>( UE::FVtValue& InValue, const int64_t& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue<uint64_t>( UE::FVtValue& InValue, const uint64_t& UnderlyingValue );
	template bool USDUTILITIES_API SetUnderlyingValue( UE::FVtValue& InValue, const std::string& UnderlyingValue );

	FString GetImpliedTypeName( const UE::FVtValue& Value )
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::VtValue& UsdValue = Value.GetUsdValue();
		pxr::SdfValueTypeName TypeName = pxr::SdfSchema::GetInstance().FindType( UsdValue.GetType() );

		return UsdToUnreal::ConvertToken( TypeName.GetAsToken() );
#else
		return FString();
#endif // USE_USD_SDK
	}
}

