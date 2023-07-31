// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"
#include "Misc/Optional.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdAttribute;
PXR_NAMESPACE_CLOSE_SCOPE

#endif // #if USE_USD_SDK

namespace UE
{
	class FSdfPath;
	class FUsdPrim;
	class FVtValue;

	namespace Internal
	{
		class FUsdAttributeImpl;
	}

	/**
	 * Minimal pxr::UsdAttribute wrapper for Unreal that can be used from no-rtti modules.
	 */
	class UNREALUSDWRAPPER_API FUsdAttribute
	{
	public:
		FUsdAttribute();

		FUsdAttribute( const FUsdAttribute& Other );
		FUsdAttribute( FUsdAttribute&& Other );
		~FUsdAttribute();

		FUsdAttribute& operator=( const FUsdAttribute& Other );
		FUsdAttribute& operator=( FUsdAttribute&& Other );

		bool operator==( const FUsdAttribute& Other ) const;
		bool operator!=( const FUsdAttribute& Other ) const;

		explicit operator bool() const;

	// Auto conversion from/to pxr::UsdAttribute
	public:
#if USE_USD_SDK
		explicit FUsdAttribute( const pxr::UsdAttribute& InUsdAttribute);
		explicit FUsdAttribute( pxr::UsdAttribute&& InUsdAttribute );
		FUsdAttribute& operator=( const pxr::UsdAttribute& InUsdAttribute );
		FUsdAttribute& operator=( pxr::UsdAttribute&& InUsdAttribute );

		operator pxr::UsdAttribute&();
		operator const pxr::UsdAttribute&() const;
#endif // #if USE_USD_SDK

	// Wrapped pxr::UsdObject functions, refer to the USD SDK documentation
	public:
		bool GetMetadata( const TCHAR* Key, UE::FVtValue& Value ) const;
		bool HasMetadata( const TCHAR* Key ) const;
		bool SetMetadata( const TCHAR* Key, const UE::FVtValue& Value ) const;
		bool ClearMetadata( const TCHAR* Key ) const;

	// Wrapped pxr::UsdAttribute functions, refer to the USD SDK documentation
	public:
		FName GetName() const;
		FName GetBaseName() const;
		FName GetTypeName() const;

		bool GetTimeSamples( TArray<double>& Times ) const;
		size_t GetNumTimeSamples() const;

		bool HasValue() const;
		bool HasFallbackValue() const;

		bool ValueMightBeTimeVarying() const;

		bool Get( UE::FVtValue& Value, TOptional<double> Time = {} ) const;
		bool Set( const UE::FVtValue& Value, TOptional<double> Time = {} ) const;

		bool Clear() const;
		bool ClearAtTime( double Time ) const;

		static bool GetUnionedTimeSamples( const TArray<UE::FUsdAttribute>& Attrs, TArray<double>& OutTimes );

		FSdfPath GetPath() const;
		FUsdPrim GetPrim() const;

	private:
		TUniquePtr< Internal::FUsdAttributeImpl > Impl;
	};
}