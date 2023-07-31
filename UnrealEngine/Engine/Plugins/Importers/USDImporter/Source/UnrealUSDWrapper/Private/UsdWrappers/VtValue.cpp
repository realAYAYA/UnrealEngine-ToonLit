// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/VtValue.h"

#include "USDMemory.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/base/vt/value.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FVtValueImpl
		{
		public:
			FVtValueImpl() = default;

#if USE_USD_SDK
			explicit FVtValueImpl( const pxr::VtValue& InVtValue )
				: PxrVtValue( InVtValue )
			{
			}

			explicit FVtValueImpl( pxr::VtValue&& InVtValue )
				: PxrVtValue( MoveTemp( InVtValue ) )
			{
			}

			TUsdStore< pxr::VtValue > PxrVtValue;
#endif // #if USE_USD_SDK
		};
	}

	FVtValue::FVtValue()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FVtValueImpl >();
	}

	FVtValue::FVtValue( const FVtValue& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl =  MakeUnique< Internal::FVtValueImpl >( Other.Impl->PxrVtValue.Get() );
#endif // #if USE_USD_SDK
	}

	FVtValue::FVtValue( FVtValue&& Other ) = default;

	FVtValue& FVtValue::operator=( const FVtValue& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FVtValueImpl >( Other.Impl->PxrVtValue.Get() );
#endif // #if USE_USD_SDK
		return *this;
	}

	FVtValue& FVtValue::operator=( FVtValue&& Other )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MoveTemp( Other.Impl );

		return *this;
	}

	FVtValue::~FVtValue()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	bool FVtValue::operator==( const FVtValue& Other ) const
	{
#if USE_USD_SDK
		return Impl->PxrVtValue.Get() == Other.Impl->PxrVtValue.Get();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FVtValue::operator!=( const FVtValue& Other ) const
	{
		return !( *this == Other );
	}

#if USE_USD_SDK
	FVtValue::FVtValue( const pxr::VtValue& InVtValue )
		: Impl( MakeUnique< Internal::FVtValueImpl >( InVtValue ) )
	{
	}

	FVtValue::FVtValue( pxr::VtValue&& InVtValue )
		: Impl( MakeUnique< Internal::FVtValueImpl >( MoveTemp( InVtValue ) ) )
	{
	}

	FVtValue& FVtValue::operator=(  const pxr::VtValue& InVtValue )
	{
		Impl = MakeUnique< Internal::FVtValueImpl >( InVtValue );
		return *this;
	}

	FVtValue& FVtValue::operator=( pxr::VtValue&& InVtValue )
	{
		Impl = MakeUnique< Internal::FVtValueImpl >( MoveTemp( InVtValue ) );
		return *this;
	}

	pxr::VtValue& FVtValue::GetUsdValue()
	{
		return Impl->PxrVtValue.Get();
	}

	const pxr::VtValue& FVtValue::GetUsdValue() const
	{
		return Impl->PxrVtValue.Get();
	}
#endif // #if USE_USD_SDK

	FString FVtValue::GetTypeName() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		return FString( ANSI_TO_TCHAR( Impl->PxrVtValue.Get().GetTypeName().c_str() ) );
#else
		return FString();
#endif // #if USE_USD_SDK
	}

	bool FVtValue::IsArrayValued() const
	{
#if USE_USD_SDK
		return Impl->PxrVtValue.Get().IsArrayValued();
#else
		return true;
#endif // #if USE_USD_SDK
	}

	bool FVtValue::IsEmpty() const
	{
#if USE_USD_SDK
		return Impl->PxrVtValue.Get().IsEmpty();
#else
		return true;
#endif // #if USE_USD_SDK
	}
}
