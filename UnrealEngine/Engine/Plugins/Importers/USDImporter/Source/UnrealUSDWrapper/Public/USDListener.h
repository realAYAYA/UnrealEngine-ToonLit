// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"

#include "UsdWrappers/ForwardDeclarations.h"
#include "UsdWrappers/VtValue.h"

class FUsdListenerImpl;

namespace UsdUtils
{
	/** Analogous to pxr::SdfChangeList::Entry::_Flags */
	struct FPrimChangeFlags
	{
		bool bDidChangeIdentifier : 1;
		bool bDidChangeResolvedPath : 1;
		bool bDidReplaceContent : 1;
		bool bDidReloadContent : 1;
		bool bDidReorderChildren : 1;
		bool bDidReorderProperties : 1;
		bool bDidRename : 1;
		bool bDidChangePrimVariantSets : 1;
		bool bDidChangePrimInheritPaths : 1;
		bool bDidChangePrimSpecializes : 1;
		bool bDidChangePrimReferences : 1;
		bool bDidChangeAttributeTimeSamples : 1;
		bool bDidChangeAttributeConnection : 1;
		bool bDidChangeRelationshipTargets : 1;
		bool bDidAddTarget : 1;
		bool bDidRemoveTarget : 1;
		bool bDidAddInertPrim : 1;
		bool bDidAddNonInertPrim : 1;
		bool bDidRemoveInertPrim : 1;
		bool bDidRemoveNonInertPrim : 1;
		bool bDidAddPropertyWithOnlyRequiredFields : 1;
		bool bDidAddProperty : 1;
		bool bDidRemovePropertyWithOnlyRequiredFields : 1;
		bool bDidRemoveProperty : 1;

		FPrimChangeFlags()
		{
			FMemory::Memset( this, 0, sizeof( *this ) );
		}
	};

	/**
	 * Analogous to pxr::SdfChangeList::Entry::InfoChange, describes a change to an attribute.
	 * Here we break off PropertyName and Field for simplicity
	 */
	struct FAttributeChange
	{
		FString PropertyName;	// metersPerUnit, kind, upAxis, etc.
		FString Field;			// default, variability, timeSamples, etc.
		UE::FVtValue OldValue;	// Can be empty when we create a new attribute opinion
		UE::FVtValue NewValue;	// Can be empty when we clear an existing attribute opinion
	};

	/** Analogous to pxr::SdfChangeList::SubLayerChangeType, describes a change to a sublayer */
	enum ESubLayerChangeType
	{
		SubLayerAdded,
		SubLayerRemoved,
		SubLayerOffset
	};

	/** Analogous to pxr::SdfChangeList::Entry, describes a generic change to an object */
	struct FObjectChangeNotice
	{
		TArray<FAttributeChange> AttributeChanges;
		FPrimChangeFlags Flags;
		FString OldPath;							// Empty if Flags.bDidRename is not set
		FString OldIdentifier;						// Empty if Flags.bDIdChangeIdentifier is not set
		TArray<TPair<FString, ESubLayerChangeType>> SubLayerChanges;
	};

	/**
	 * Describes USD object changes by object path.
	 * The only difference to the USD data structures is that we store them by object path here for convenience, so
	 * the key can be "/" to signify a stage change, or something like "/MyRoot/SomePrim" to indicate a particular prim change
	 */
	using FObjectChangesByPath = TMap<FString, TArray<FObjectChangeNotice>>;
}

/**
 * Registers to Usd Notices and emits events when the Usd Stage has changed
 */
class UNREALUSDWRAPPER_API FUsdListener
{
public:
	FUsdListener();
	FUsdListener( const UE::FUsdStage& Stage );

	FUsdListener( const FUsdListener& Other ) = delete;
	FUsdListener( FUsdListener&& Other ) = delete;

	virtual ~FUsdListener();

	FUsdListener& operator=( const FUsdListener& Other ) = delete;
	FUsdListener& operator=( FUsdListener&& Other ) = delete;

	void Register( const UE::FUsdStage& Stage );

	// Increment/decrement the block counter
	void Block();
	void Unblock();
	bool IsBlocked() const;

	DECLARE_EVENT( FUsdListener, FOnStageEditTargetChanged );
	FOnStageEditTargetChanged& GetOnStageEditTargetChanged();

	DECLARE_EVENT_OneParam( FUsdListener, FOnLayersChanged, const TArray< FString >& );
	FOnLayersChanged& GetOnLayersChanged();

	DECLARE_EVENT_TwoParams( FUsdListener, FOnObjectsChanged, const UsdUtils::FObjectChangesByPath& /* InfoChanges */, const UsdUtils::FObjectChangesByPath& /* ResyncChanges */ );
	FOnObjectsChanged& GetOnObjectsChanged();

private:
	TUniquePtr< FUsdListenerImpl > Impl;
};

class UNREALUSDWRAPPER_API FScopedBlockNotices final
{
public:
	explicit FScopedBlockNotices( FUsdListener& InListener );
	~FScopedBlockNotices();

	FScopedBlockNotices() = delete;
	FScopedBlockNotices( const FScopedBlockNotices& ) = delete;
	FScopedBlockNotices( FScopedBlockNotices&& ) = delete;
	FScopedBlockNotices& operator=( const FScopedBlockNotices& ) = delete;
	FScopedBlockNotices& operator=( FScopedBlockNotices&& ) = delete;

private:
	FUsdListener& Listener;
};
