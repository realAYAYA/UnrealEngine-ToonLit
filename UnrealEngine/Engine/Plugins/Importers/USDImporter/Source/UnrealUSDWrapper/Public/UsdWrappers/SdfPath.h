// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class SdfPath;
PXR_NAMESPACE_CLOSE_SCOPE

#endif // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FSdfPathImpl;
	}

	/**
	 * Minimal pxr::SdfPath wrapper for Unreal that can be used from no-rtti modules.
	 */
	class UNREALUSDWRAPPER_API FSdfPath
	{
	public:
		FSdfPath();
		explicit FSdfPath( const TCHAR* InPath );

		FSdfPath( const FSdfPath& Other );
		FSdfPath( FSdfPath&& Other );

		FSdfPath& operator=( const FSdfPath& Other );
		FSdfPath& operator=( FSdfPath&& Other );

		~FSdfPath();

		bool operator==( const FSdfPath& Other ) const;
		bool operator!=( const FSdfPath& Other ) const;

		friend UNREALUSDWRAPPER_API uint32 GetTypeHash( const UE::FSdfPath& Path );
		friend UNREALUSDWRAPPER_API FArchive& operator<<( FArchive& Ar, FSdfPath& Path );

	// Auto conversion from/to pxr::SdfPath
	public:
#if USE_USD_SDK
		explicit FSdfPath( const pxr::SdfPath& InSdfPath );
		explicit FSdfPath( pxr::SdfPath&& InSdfPath );
		FSdfPath& operator=(  const pxr::SdfPath& InSdfPath );
		FSdfPath& operator=( pxr::SdfPath&& InSdfPath );

		operator pxr::SdfPath&();
		operator const pxr::SdfPath&() const;
#endif // #if USE_USD_SDK

	// Wrapped pxr::SdfPath functions, refer to the USD SDK documentation
	public:
		static const FSdfPath& AbsoluteRootPath();

		bool IsEmpty() const noexcept;

		bool IsAbsoluteRootPath() const;
		bool IsAbsoluteRootOrPrimPath() const;
		FString GetName() const;
		FString GetElementString() const;
		FSdfPath GetAbsoluteRootOrPrimPath() const;

		FSdfPath ReplaceName( const TCHAR* NewLeafName ) const;

		FSdfPath GetParentPath() const;
		FSdfPath AppendChild( const TCHAR* ChildName ) const;
		FSdfPath AppendProperty( FName PropertyName ) const;

		FSdfPath StripAllVariantSelections() const;

		FString GetString() const;

		TArray<FSdfPath> GetPrefixes() const;

	private:
		TUniquePtr< Internal::FSdfPathImpl > Impl;
	};
}