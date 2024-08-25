// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdPrim;
	class UsdGeomBBoxCache;
PXR_NAMESPACE_CLOSE_SCOPE

#endif	  // #if USE_USD_SDK

enum class EUsdPurpose : int32;

namespace UE
{
	namespace Internal
	{
		class FUsdGeomBBoxCacheImpl;
	}

	/**
	 * Minimal pxr::UsdGeomBBoxCache wrapper for Unreal that can be used from no-rtti modules.
	 * Unlike the other wrapper types, FUsdGeomBBoxCache contains its own mutex in order to try
	 * and make its usage somewhat thread-safe. The wrapped functions will use this mutex already,
	 * and the conversion operators that return the underlying type will check that the mutex is
	 * locked before returning (the caller is expected to lock the mutex before using the operators).
	 */
	class UNREALUSDWRAPPER_API FUsdGeomBBoxCache
	{
	public:
		FUsdGeomBBoxCache(double Time, EUsdPurpose IncludedPurposeFlags, bool bUseExtentsHint = false, bool bIgnoreVisibility = false);
		FUsdGeomBBoxCache(const FUsdGeomBBoxCache& Other);
		FUsdGeomBBoxCache(FUsdGeomBBoxCache&& Other);

		FUsdGeomBBoxCache& operator=(const FUsdGeomBBoxCache& Other);
		FUsdGeomBBoxCache& operator=(FUsdGeomBBoxCache&& Other);

		~FUsdGeomBBoxCache();

		// Auto conversion from/to pxr::UsdGeomBBoxCache
	public:
#if USE_USD_SDK
		explicit FUsdGeomBBoxCache(const pxr::UsdGeomBBoxCache& InUsdGeomBBoxCache);
		explicit FUsdGeomBBoxCache(pxr::UsdGeomBBoxCache&& InUsdGeomBBoxCache);

		operator pxr::UsdGeomBBoxCache&();
		operator const pxr::UsdGeomBBoxCache&() const;
#endif	  // #if USE_USD_SDK

		  // Wrapped pxr::UsdGeomBBoxCache functions, refer to the USD SDK documentation
	public:
		void Clear();

		void SetIncludedPurposes(EUsdPurpose IncludedPurposeFlags);
		EUsdPurpose GetIncludedPurposes() const;

		bool GetUseExtentsHint() const;
		bool GetIgnoreVisibility() const;

		void SetTime(double UsdTimeCode);
		double GetTime() const;

	public:
		// The underlying pxr::UsdGeomBBoxCache object is not thread-safe, so we have this.
		// This mutex will be internally locked for all of the wrapped function calls.
		// It is exposed so that it can be locked by the user in case it is desirable to use
		// the underlying pxr::UsdGeomBBoxCache type directly
		mutable FRWLock Lock;

	private:
		TUniquePtr<Internal::FUsdGeomBBoxCacheImpl> Impl;
	};
}	 // namespace UE
