// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/UsdAttribute.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/VtValue.h"

#include "USDMemory.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/usd/attribute.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FUsdAttributeImpl
		{
		public:
			FUsdAttributeImpl() = default;

#if USE_USD_SDK
			explicit FUsdAttributeImpl( const pxr::UsdAttribute& InUsdAttribute )
				: PxrUsdAttribute( InUsdAttribute )
			{
			}

			explicit FUsdAttributeImpl( pxr::UsdAttribute&& InUsdAttribute )
				: PxrUsdAttribute( MoveTemp( InUsdAttribute ) )
			{
			}

			TUsdStore< pxr::UsdAttribute > PxrUsdAttribute;
#endif // #if USE_USD_SDK
		};
	}
}

namespace UE
{
	FUsdAttribute::FUsdAttribute()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl =  MakeUnique< Internal::FUsdAttributeImpl >();
	}

	FUsdAttribute::FUsdAttribute( const FUsdAttribute& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdAttributeImpl >( Other.Impl->PxrUsdAttribute.Get() );
#endif // #if USE_USD_SDK
	}

	FUsdAttribute::FUsdAttribute( FUsdAttribute&& Other ) = default;

	FUsdAttribute::~FUsdAttribute()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	FUsdAttribute& FUsdAttribute::operator=( const FUsdAttribute& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdAttributeImpl >(  Other.Impl->PxrUsdAttribute.Get() );
#endif // #if USE_USD_SDK
		return *this;
	}

	FUsdAttribute& FUsdAttribute::operator=( FUsdAttribute&& Other )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MoveTemp( Other.Impl );

		return *this;
	}

	bool FUsdAttribute::operator==( const FUsdAttribute& Other ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get() == Other.Impl->PxrUsdAttribute.Get();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::operator!=( const FUsdAttribute& Other ) const
	{
		return !( *this == Other );
	}

	FUsdAttribute::operator bool() const
	{
#if USE_USD_SDK
		return (bool)Impl->PxrUsdAttribute.Get();
#else
		return false;
#endif // #if USE_USD_SDK
	}

#if USE_USD_SDK
	FUsdAttribute::FUsdAttribute( const pxr::UsdAttribute& InUsdAttribute )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl =  MakeUnique< Internal::FUsdAttributeImpl >( InUsdAttribute );
	}

	FUsdAttribute::FUsdAttribute( pxr::UsdAttribute&& InUsdAttribute )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdAttributeImpl >( MoveTemp( InUsdAttribute ) );
	}

	FUsdAttribute& FUsdAttribute::operator=( const pxr::UsdAttribute& InUsdAttribute )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdAttributeImpl >( InUsdAttribute );
		return *this;
	}

	FUsdAttribute& FUsdAttribute::operator=( pxr::UsdAttribute&& InUsdAttribute )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdAttributeImpl >( MoveTemp( InUsdAttribute ) );
		return *this;
	}

	FUsdAttribute::operator pxr::UsdAttribute&()
	{
		return Impl->PxrUsdAttribute.Get();
	}

	FUsdAttribute::operator const pxr::UsdAttribute&() const
	{
		return Impl->PxrUsdAttribute.Get();
	}
#endif // #if USE_USD_SDK

	bool FUsdAttribute::GetMetadata( const TCHAR* Key, UE::FVtValue& Value ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().GetMetadata( pxr::TfToken{ TCHAR_TO_ANSI( Key ) }, &Value.GetUsdValue() );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasMetadata( const TCHAR* Key ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasMetadata( pxr::TfToken{ TCHAR_TO_ANSI( Key ) } );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::SetMetadata( const TCHAR* Key, const UE::FVtValue& Value ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().SetMetadata( pxr::TfToken{ TCHAR_TO_ANSI( Key ) }, Value.GetUsdValue() );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::ClearMetadata( const TCHAR* Key ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ClearMetadata( pxr::TfToken{ TCHAR_TO_ANSI( Key ) } );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetName() const
	{
#if USE_USD_SDK
		return FName( ANSI_TO_TCHAR( Impl->PxrUsdAttribute.Get().GetName().GetString().c_str() ) );
#else
		return FName();
#endif // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetBaseName() const
	{
#if USE_USD_SDK
		return FName( ANSI_TO_TCHAR( Impl->PxrUsdAttribute.Get().GetBaseName().GetString().c_str() ) );
#else
		return FName();
#endif // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetTypeName() const
	{
#if USE_USD_SDK
		return FName( ANSI_TO_TCHAR( Impl->PxrUsdAttribute.Get().GetTypeName().GetAsToken().GetString().c_str() ) );
#else
		return FName();
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::GetTimeSamples( TArray<double>& Times ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		std::vector<double> UsdTimes;
		bool bResult = Impl->PxrUsdAttribute.Get().GetTimeSamples( &UsdTimes );
		if ( !bResult )
		{
			return false;
		}

		Times.SetNumUninitialized( UsdTimes.size() );
		FMemory::Memcpy( Times.GetData(), UsdTimes.data(), UsdTimes.size() * sizeof( double ) );

		return true;
#else
		return false;
#endif // #if USE_USD_SDK
	}

	size_t FUsdAttribute::GetNumTimeSamples() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().GetNumTimeSamples();
#else
		return 0;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasValue() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasValue();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasFallbackValue() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasFallbackValue();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::ValueMightBeTimeVarying() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ValueMightBeTimeVarying();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::Get( UE::FVtValue & Value, TOptional<double> Time /*= {} */ ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdTimeCode TimeCode = Time.IsSet() ? Time.GetValue() : pxr::UsdTimeCode::Default();
		return Impl->PxrUsdAttribute.Get().Get( &Value.GetUsdValue(), TimeCode );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::Set( const UE::FVtValue & Value, TOptional<double> Time /*= {} */ ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdTimeCode TimeCode = Time.IsSet() ? Time.GetValue() : pxr::UsdTimeCode::Default();
		return Impl->PxrUsdAttribute.Get().Set( Value.GetUsdValue(), TimeCode );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::Clear() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().Clear();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::ClearAtTime( double Time ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ClearAtTime( pxr::UsdTimeCode( Time ) );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdAttribute::GetUnionedTimeSamples( const TArray<UE::FUsdAttribute>& Attrs, TArray<double>& OutTimes )
	{
		bool bResult = false;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		std::vector<pxr::UsdAttribute> UsdAttrs;
		UsdAttrs.reserve( Attrs.Num() );
		for ( const UE::FUsdAttribute& Attr : Attrs )
		{
			UsdAttrs.push_back( Attr );
		}

		std::vector<double> UsdTimes;

		bResult = pxr::UsdAttribute::GetUnionedTimeSamples( UsdAttrs, &UsdTimes );
		if ( bResult )
		{
			OutTimes.SetNumUninitialized( UsdTimes.size() );
			FMemory::Memcpy( OutTimes.GetData(), UsdTimes.data(), OutTimes.Num() * OutTimes.GetTypeSize() );
		}
#endif // #if USE_USD_SDK

		return bResult;
	}

	FSdfPath FUsdAttribute::GetPath() const
	{
#if USE_USD_SDK
		return FSdfPath( Impl->PxrUsdAttribute.Get().GetPath() );
#else
		return FSdfPath();
#endif // #if USE_USD_SDK
	}

	FUsdPrim FUsdAttribute::GetPrim() const
	{
#if USE_USD_SDK
		return FUsdPrim( Impl->PxrUsdAttribute.Get().GetPrim() );
#else
		return FUsdPrim();
#endif // #if USE_USD_SDK
	}
}
