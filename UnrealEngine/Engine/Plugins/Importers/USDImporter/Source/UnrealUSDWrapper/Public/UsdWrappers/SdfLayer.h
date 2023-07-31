// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UsdWrappers/ForwardDeclarations.h"

namespace UE
{
	class FSdfPath;
	class FSdfPrimSpec;

	namespace Internal
	{
		template < typename PtrType > class FSdfLayerImpl;
	}

	struct UNREALUSDWRAPPER_API FSdfLayerOffset
	{
		FSdfLayerOffset() = default;
		FSdfLayerOffset( double InOffset, double InScale )
			: Offset( InOffset )
			, Scale( InScale )
		{
		}

		double Offset = 0.0;
		double Scale = 1.0;
	};

	/**
	 * Minimal pxr::SdfLayer pointer wrapper for Unreal that can be used from no-rtti modules.
	 * Use the aliases FSdfLayer and FSdfLayerWeak instead (defined on ForwardDeclarations.h)
	 */
	template< typename PtrType >
	class UNREALUSDWRAPPER_API FSdfLayerBase
	{
	public:
		FSdfLayerBase();

		FSdfLayerBase( const FSdfLayer& Other );
		FSdfLayerBase( FSdfLayer&& Other );
		FSdfLayerBase( const FSdfLayerWeak& Other );
		FSdfLayerBase( FSdfLayerWeak&& Other );

		~FSdfLayerBase();

		FSdfLayerBase& operator=( const FSdfLayer& Other );
		FSdfLayerBase& operator=( FSdfLayer&& Other );
		FSdfLayerBase& operator=( const FSdfLayerWeak& Other );
		FSdfLayerBase& operator=( FSdfLayerWeak&& Other );

		bool operator==( const FSdfLayerBase& Other ) const;
		bool operator!=( const FSdfLayerBase& Other ) const;

		explicit operator bool() const;

		// Auto conversion from/to PtrType. We use concrete pointer types here
		// because we should also be able to convert between them
	public:
#if USE_USD_SDK
		explicit FSdfLayerBase( const pxr::SdfLayerRefPtr& InSdfLayer );
		explicit FSdfLayerBase( pxr::SdfLayerRefPtr&& InSdfLayer );
		explicit FSdfLayerBase( const pxr::SdfLayerWeakPtr& InSdfLayer );
		explicit FSdfLayerBase( pxr::SdfLayerWeakPtr&& InSdfLayer );

		FSdfLayerBase& operator=( const pxr::SdfLayerRefPtr& InSdfLayer );
		FSdfLayerBase& operator=( pxr::SdfLayerRefPtr&& InSdfLayer );
		FSdfLayerBase& operator=( const pxr::SdfLayerWeakPtr& InSdfLayer );
		FSdfLayerBase& operator=( pxr::SdfLayerWeakPtr&& InSdfLayer );

		// We can provide reference cast operators for the type we do have, but
		// need to settle on providing copy cast operators for the other types
		operator PtrType&();
		operator const PtrType&() const;

		operator pxr::SdfLayerRefPtr() const;
		operator pxr::SdfLayerWeakPtr() const;
#endif // #if USE_USD_SDK

	// Wrapped pxr::SdfLayer functions, refer to the USD SDK documentation
	public:
		FString GetComment() const;
		void SetComment( const TCHAR* Comment ) const;

		void TransferContent( const FSdfLayer& SourceLayer );

		static FSdfLayer FindOrOpen( const TCHAR* Identifier );
		static FSdfLayer CreateNew( const TCHAR* Identifier );

		bool Save( bool bForce = false ) const;

		TSet<FString> GetCompositionAssetDependencies() const;

		bool UpdateCompositionAssetDependency(
			const TCHAR* OldAssetPath,
			const TCHAR* NewAssetPath = nullptr);

		FString GetRealPath() const;
		FString GetIdentifier() const;
		FString GetDisplayName() const;

		FString ComputeAbsolutePath(const FString& AssetPath) const;

		bool IsDirty() const;
		bool IsEmpty() const;
		bool IsAnonymous() const;

		bool Export( const TCHAR* Filename ) const;

		void Clear();

		bool HasStartTimeCode() const;
		double GetStartTimeCode() const;
		void SetStartTimeCode( double TimeCode );

		bool HasEndTimeCode() const;
		double GetEndTimeCode() const;
		void SetEndTimeCode( double TimeCode );

		bool HasTimeCodesPerSecond() const;
		double GetTimeCodesPerSecond() const;
		void SetTimeCodesPerSecond( double TimeCodesPerSecond );

		bool HasFramesPerSecond() const;
		double GetFramesPerSecond() const;
		void RemoveSubLayerPath( int32 Index );
		void SetFramesPerSecond( double FramesPerSecond );

		int64 GetNumSubLayerPaths() const;
		TArray< FString > GetSubLayerPaths() const;
		TArray< FSdfLayerOffset > GetSubLayerOffsets() const;

		void SetSubLayerOffset( const FSdfLayerOffset& LayerOffset, int32 Index );

		bool HasSpec( const FSdfPath& Path ) const;

		FSdfPrimSpec GetPseudoRoot() const;
		FSdfPrimSpec GetPrimAtPath( const FSdfPath& Path ) const;

		TSet< double > ListTimeSamplesForPath( const FSdfPath& Path ) const;
		void EraseTimeSample( const FSdfPath& Path, double Time );

		bool IsMuted() const;
		void SetMuted(bool bMuted);

	private:
		friend FSdfLayer;
		friend FSdfLayerWeak;

		TUniquePtr< Internal::FSdfLayerImpl<PtrType> > Impl;
	};

	/**
	 * Wrapper for global functions in pxr/usd/sdf/layerUtils.h
	 */
	class UNREALUSDWRAPPER_API FSdfLayerUtils
	{
	public:
		static FString SdfComputeAssetPathRelativeToLayer( const FSdfLayer& Anchor, const TCHAR* AssetPath );
	};
}