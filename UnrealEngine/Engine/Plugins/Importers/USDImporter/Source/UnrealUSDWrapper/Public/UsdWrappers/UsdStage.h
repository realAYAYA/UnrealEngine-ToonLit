// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

#include "UnrealUSDWrapper.h"
#include "UsdWrappers/ForwardDeclarations.h"

namespace UE
{
	class FSdfPath;
	class FUsdPrim;
	class FVtValue;

	namespace Internal
	{
		template< typename PtrType > class FUsdStageImpl;
	}

	/** Corresponds to pxr::UsdLoadPolicy, refer to the USD SDK documentation */
	enum class EUsdLoadPolicy
	{
		UsdLoadWithDescendants,    // Load a prim plus all its descendants.
		UsdLoadWithoutDescendants  // Load a prim by itself with no descendants.
	};

	/**
	 * Minimal pxr::UsdStage pointer wrapper for Unreal that can be used from no-rtti modules.
	 * Use the aliases FUsdStage and FUsdStageWeak instead (defined on ForwardDeclarations.h)
	 */
	template< typename PtrType >
	class UNREALUSDWRAPPER_API FUsdStageBase
	{
	public:
		FUsdStageBase();

		FUsdStageBase( const FUsdStage& Other );
		FUsdStageBase( FUsdStage&& Other );
		FUsdStageBase( const FUsdStageWeak& Other );
		FUsdStageBase( FUsdStageWeak&& Other );

		FUsdStageBase& operator=( const FUsdStage& Other );
		FUsdStageBase& operator=( FUsdStage&& Other );
		FUsdStageBase& operator=( const FUsdStageWeak& Other );
		FUsdStageBase& operator=( FUsdStageWeak&& Other );

		~FUsdStageBase();

		explicit operator bool() const;

		bool operator==( const FUsdStageBase& Other ) const;
		bool operator!=( const FUsdStageBase& Other ) const;

		// Auto conversion from/to PtrType. We use concrete pointer types here
		// because we should also be able to convert between them
	public:
#if USE_USD_SDK
		explicit FUsdStageBase( const pxr::UsdStageRefPtr& InUsdPtr );
		explicit FUsdStageBase( pxr::UsdStageRefPtr&& InUsdPtr );
		explicit FUsdStageBase( const pxr::UsdStageWeakPtr& InUsdPtr );
		explicit FUsdStageBase( pxr::UsdStageWeakPtr&& InUsdPtr );

		operator PtrType&();
		operator const PtrType&() const;

		operator pxr::UsdStageRefPtr ( ) const;
		operator pxr::UsdStageWeakPtr ( ) const;
#endif // USE_USD_SDK

	// Wrapped pxr::UsdStage functions, refer to the USD SDK documentation
	public:
		void LoadAndUnload( const TSet<UE::FSdfPath>& LoadSet, const TSet<UE::FSdfPath>& UnloadSet, EUsdLoadPolicy Policy = EUsdLoadPolicy::UsdLoadWithDescendants );

		/**
		 * Saves a flattened copy of the stage to the given path (e.g. "C:/Folder/FlattenedStage.usda"). Will use the corresponding file writer depending on FilePath extension.
		 * Will not alter the current stage.
		 */
		bool Export( const TCHAR* FileName, bool bAddSourceFileComment = true, const TMap<FString, FString>& FileFormatArguments = {} ) const;

		FSdfLayer GetRootLayer() const;
		FSdfLayer GetSessionLayer() const;
		bool HasLocalLayer( const FSdfLayer& Layer ) const;

		FUsdPrim GetPseudoRoot() const;
		FUsdPrim GetDefaultPrim() const;
		FUsdPrim GetPrimAtPath( const FSdfPath& Path ) const;

		TArray<FSdfLayer> GetLayerStack( bool bIncludeSessionLayers = true ) const;
		TArray<FSdfLayer> GetUsedLayers( bool bIncludeClipLayers = true ) const;

		void MuteAndUnmuteLayers( const TArray<FString>& MuteLayers, const TArray<FString>& UnmuteLayers );
		bool IsLayerMuted( const FString& LayerIdentifier ) const;

		bool IsEditTargetValid() const;
		void SetEditTarget( const FSdfLayer& Layer );
		FSdfLayer GetEditTarget() const;

		bool GetMetadata( const TCHAR* Key, UE::FVtValue& Value ) const;
		bool HasMetadata( const TCHAR* Key ) const;
		bool SetMetadata( const TCHAR* Key, const UE::FVtValue& Value ) const;
		bool ClearMetadata( const TCHAR* Key ) const;

		double GetStartTimeCode() const;
		double GetEndTimeCode() const;
		void SetStartTimeCode( double TimeCode );
		void SetEndTimeCode( double TimeCode );
		double GetTimeCodesPerSecond() const;
		void SetTimeCodesPerSecond( double TimeCodesPerSecond );
		double GetFramesPerSecond() const;
		void SetFramesPerSecond( double FramesPerSecond );

		void SetInterpolationType( EUsdInterpolationType InterpolationType );
		EUsdInterpolationType GetInterpolationType() const;

		void SetDefaultPrim( const FUsdPrim& Prim );

		FUsdPrim OverridePrim( const FSdfPath& Path );
		FUsdPrim DefinePrim( const FSdfPath& Path, const TCHAR* TypeName = TEXT("") );
		bool RemovePrim( const FSdfPath& Path );

	private:
		// So we can use the Other's Impl on copy constructor/operators
		friend FUsdStage;
		friend FUsdStageWeak;

		TUniquePtr< Internal::FUsdStageImpl<PtrType> > Impl;
	};
}