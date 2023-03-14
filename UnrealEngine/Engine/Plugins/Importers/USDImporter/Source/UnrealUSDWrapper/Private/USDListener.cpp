// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDListener.h"

#include "USDLog.h"
#include "USDMemory.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/VtValue.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"

#include "pxr/base/tf/weakBase.h"
#include "pxr/usd/sdf/changeList.h"
#include "pxr/usd/sdf/notice.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/notice.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/tokens.h"

#include "USDIncludesEnd.h"

#endif // USE_USD_SDK

namespace UsdToUnreal
{
#if USE_USD_SDK
	bool ConvertPathRange( const pxr::UsdNotice::ObjectsChanged::PathRange& PathRange, UsdUtils::FObjectChangesByPath& OutChanges )
	{
		using namespace pxr;

		FScopedUsdAllocs UsdAllocs;

		OutChanges.Reset();

		for ( UsdNotice::ObjectsChanged::PathRange::const_iterator It = PathRange.begin(); It != PathRange.end(); ++It )
		{
			const FString PrimPath = ANSI_TO_TCHAR( It->GetAbsoluteRootOrPrimPath().GetAsString().c_str() );

			if (pxr::UsdPrim::IsPathInPrototype(UE::FSdfPath(*PrimPath)))
			{
				continue;
			}

			// Something like "/Root/Prim.some_field", or "/"
			const FString FullFieldPath = ANSI_TO_TCHAR( It->GetAsString().c_str() );

			TArray<UsdUtils::FObjectChangeNotice>& ConvertedChanges = OutChanges.FindOrAdd( PrimPath );

			// Changes may be empty, but we should still pass along this overall notice because sending a root
			// resync notice with no actual change item inside is how USD signals that a layer has been added/removed/resynced
			const std::vector<const SdfChangeList::Entry*>& Changes = It.base()->second;
			for ( const SdfChangeList::Entry* Entry : Changes )
			{
				UsdUtils::FObjectChangeNotice& ConvertedEntry = ConvertedChanges.Emplace_GetRef();

				// For most changes we'll only get one of these, but sometimes multiple changes are fired in sequence
				// (e.g. if you change framesPerSecond, it will send a notice for it but also for the matching, updated timeCodesPerSecond)
				for ( const std::pair<TfToken, std::pair<VtValue, VtValue>>& AttributeChange : Entry->infoChanged ) // Note: infoChanged here is just a naming conflict, there's no "resyncChanged"
				{
					UsdUtils::FAttributeChange& ConvertedAttributeChange = ConvertedEntry.AttributeChanges.Emplace_GetRef();

					const FString FieldToken = ANSI_TO_TCHAR( AttributeChange.first.GetString().c_str() );

					// For regular properties (most common case) FullFieldPath will already carry the property and FieldToken will be "default", "timeSamples", "variability", etc.
					if ( It->IsPropertyPath() )
					{
						ConvertedAttributeChange.PropertyName = ANSI_TO_TCHAR( It->GetName().c_str() );
						ConvertedAttributeChange.Field = FieldToken;
					}
					// For stage properties it seems like USD likes to send just "/" as the FullFieldPath, and the actual property name in FieldToken (e.g. "metersPerUnit", "timeCodesPerSecond", etc.)
					// We send "default" here for consistency
					else if ( It->IsAbsoluteRootPath() || FieldToken == FString( TEXT( "kind" ) ) )
					{
						ConvertedAttributeChange.PropertyName = FieldToken;
						ConvertedAttributeChange.Field = TEXT( "default" );
					}

					ConvertedAttributeChange.OldValue = UE::FVtValue{ AttributeChange.second.first };
					ConvertedAttributeChange.NewValue = UE::FVtValue{ AttributeChange.second.second };
				}

				// Some notices (like creating/removing a property) don't have any actual infoChanged entries, so we need to create a fake one in here in order to convey the PropertyName, if applicable
				if ( Entry->infoChanged.size() == 0 &&
					( Entry->flags.didAddProperty ||
					  Entry->flags.didAddPropertyWithOnlyRequiredFields ||
					  Entry->flags.didRemoveProperty ||
					  Entry->flags.didRemovePropertyWithOnlyRequiredFields ||
					  Entry->flags.didChangeAttributeTimeSamples ) )
				{
					UsdUtils::FAttributeChange& ConvertedAttributeChange = ConvertedEntry.AttributeChanges.Emplace_GetRef();
					ConvertedAttributeChange.Field = Entry->flags.didChangeAttributeTimeSamples ? TEXT( "timeSamples" ) : TEXT( "default" );

					if ( It->IsPropertyPath() )
					{
						ConvertedAttributeChange.PropertyName = ANSI_TO_TCHAR( It->GetName().c_str() );
					}
				}

				// These should be packed just the same, but just in case we do member by member here instead of memcopying it over
				UsdUtils::FPrimChangeFlags& ConvertedFlags = ConvertedEntry.Flags;
				ConvertedFlags.bDidChangeIdentifier = Entry->flags.didChangeIdentifier;
				ConvertedFlags.bDidChangeResolvedPath = Entry->flags.didChangeResolvedPath;
				ConvertedFlags.bDidReplaceContent = Entry->flags.didReplaceContent;
				ConvertedFlags.bDidReloadContent = Entry->flags.didReloadContent;
				ConvertedFlags.bDidReorderChildren = Entry->flags.didReorderChildren;
				ConvertedFlags.bDidReorderProperties = Entry->flags.didReorderProperties;
				ConvertedFlags.bDidRename = Entry->flags.didRename;
				ConvertedFlags.bDidChangePrimVariantSets = Entry->flags.didChangePrimVariantSets;
				ConvertedFlags.bDidChangePrimInheritPaths = Entry->flags.didChangePrimInheritPaths;
				ConvertedFlags.bDidChangePrimSpecializes = Entry->flags.didChangePrimSpecializes;
				ConvertedFlags.bDidChangePrimReferences = Entry->flags.didChangePrimReferences;
				ConvertedFlags.bDidChangeAttributeTimeSamples = Entry->flags.didChangeAttributeTimeSamples;
				ConvertedFlags.bDidChangeAttributeConnection = Entry->flags.didChangeAttributeConnection;
				ConvertedFlags.bDidChangeRelationshipTargets = Entry->flags.didChangeRelationshipTargets;
				ConvertedFlags.bDidAddTarget = Entry->flags.didAddTarget;
				ConvertedFlags.bDidRemoveTarget = Entry->flags.didRemoveTarget;
				ConvertedFlags.bDidAddInertPrim = Entry->flags.didAddInertPrim;
				ConvertedFlags.bDidAddNonInertPrim = Entry->flags.didAddNonInertPrim;
				ConvertedFlags.bDidRemoveInertPrim = Entry->flags.didRemoveInertPrim;
				ConvertedFlags.bDidRemoveNonInertPrim = Entry->flags.didRemoveNonInertPrim;
				ConvertedFlags.bDidAddPropertyWithOnlyRequiredFields = Entry->flags.didAddPropertyWithOnlyRequiredFields;
				ConvertedFlags.bDidAddProperty = Entry->flags.didAddProperty;
				ConvertedFlags.bDidRemovePropertyWithOnlyRequiredFields = Entry->flags.didRemovePropertyWithOnlyRequiredFields;
				ConvertedFlags.bDidRemoveProperty = Entry->flags.didRemoveProperty;

				static_assert( static_cast< int >( UsdUtils::ESubLayerChangeType::SubLayerAdded ) == static_cast< int >( pxr::SdfChangeList::SubLayerChangeType::SubLayerAdded ), "Enum values changed!" );
				static_assert( static_cast< int >( UsdUtils::ESubLayerChangeType::SubLayerOffset ) == static_cast< int >( pxr::SdfChangeList::SubLayerChangeType::SubLayerOffset ), "Enum values changed!" );
				static_assert( static_cast< int >( UsdUtils::ESubLayerChangeType::SubLayerRemoved ) == static_cast< int >( pxr::SdfChangeList::SubLayerChangeType::SubLayerRemoved ), "Enum values changed!" );
				for ( const std::pair<std::string, pxr::SdfChangeList::SubLayerChangeType>& SubLayerChange : Entry->subLayerChanges )
				{
					ConvertedEntry.SubLayerChanges.Add(
						TPair<FString, UsdUtils::ESubLayerChangeType>(
							ANSI_TO_TCHAR( SubLayerChange.first.c_str() ),
							static_cast< UsdUtils::ESubLayerChangeType >( SubLayerChange.second )
						)
					);
				}

				ConvertedEntry.OldPath = ANSI_TO_TCHAR( Entry->oldPath.GetString().c_str() );
				ConvertedEntry.OldIdentifier = ANSI_TO_TCHAR( Entry->oldIdentifier.c_str() );
			}
		}

		return true;
	}

	bool ConvertObjectsChangedNotice( const pxr::UsdNotice::ObjectsChanged& InNotice, UsdUtils::FObjectChangesByPath& OutInfoChanges, UsdUtils::FObjectChangesByPath& OutResyncChanges)
	{
		ConvertPathRange( InNotice.GetChangedInfoOnlyPaths(), OutInfoChanges );
		ConvertPathRange( InNotice.GetResyncedPaths(), OutResyncChanges );

		for ( TPair< FString, TArray<UsdUtils::FObjectChangeNotice> >& InfoPair : OutInfoChanges )
		{
			const FString& PrimPath = InfoPair.Key;
			TArray< UsdUtils::FObjectChangeNotice >* AnalogueResyncChanges = nullptr;

			for ( TArray<UsdUtils::FObjectChangeNotice>::TIterator ChangeIt = InfoPair.Value.CreateIterator(); ChangeIt; ++ChangeIt )
			{
				bool bUpgrade = false;

				// Upgrade info changes with content reloads into resync changes
				if ( ChangeIt->Flags.bDidReloadContent )
				{
					bUpgrade = true;
				}

				// Upgrade visibility changes to resyncs because in case of mesh collapsing having one of the collapsed meshes go visible/invisible
				// should cause the regeneration of the collapsed asset. This is a bit expensive, but the asset cache will be used so its not as if
				// the mesh will be completely regenerated however
				if ( !bUpgrade )
				{
					for ( UsdUtils::FAttributeChange& Change : ChangeIt->AttributeChanges )
					{
						if ( Change.PropertyName == ANSI_TO_TCHAR( pxr::UsdGeomTokens->visibility.GetString().c_str() ) )
						{
							bUpgrade = true;
							break;
						}
					}
				}

				if ( bUpgrade )
				{
					if ( !AnalogueResyncChanges )
					{
						AnalogueResyncChanges = &OutResyncChanges.FindOrAdd( PrimPath );
					}

					AnalogueResyncChanges->Add( *ChangeIt );
					ChangeIt.RemoveCurrent();
				}
			}
		}

		return true;
	}
#endif // USE_USD_SDK
}

class FUsdListenerImpl
#if USE_USD_SDK
	: public pxr::TfWeakBase
#endif // #if USE_USD_SDK
{
public:
	FUsdListenerImpl() = default;

	virtual ~FUsdListenerImpl();

	FUsdListener::FOnStageEditTargetChanged OnStageEditTargetChanged;
	FUsdListener::FOnLayersChanged OnLayersChanged;
	FUsdListener::FOnObjectsChanged OnObjectsChanged;

	FThreadSafeCounter IsBlocked;

#if USE_USD_SDK
	FUsdListenerImpl( const pxr::UsdStageRefPtr& Stage );

	void Register( const pxr::UsdStageRefPtr& Stage );

protected:
	void HandleObjectsChangedNotice( const pxr::UsdNotice::ObjectsChanged& Notice, const pxr::UsdStageWeakPtr& Sender );
	void HandleStageEditTargetChangedNotice( const pxr::UsdNotice::StageEditTargetChanged& Notice, const pxr::UsdStageWeakPtr& Sender );
	void HandleLayersChangedNotice( const pxr::SdfNotice::LayersDidChange& Notice );
	void HandleLayerDirtinessChangedNotice( const pxr::SdfNotice::LayerDirtinessChanged& Notice );

private:
	pxr::TfNotice::Key RegisteredObjectsChangedKey;
	pxr::TfNotice::Key RegisteredStageEditTargetChangedKey;
	pxr::TfNotice::Key RegisteredLayersChangedKey;
	pxr::TfNotice::Key RegisteredLayerDirtinessChangedKey;
#endif // #if USE_USD_SDK
};

FUsdListener::FUsdListener()
	: Impl( MakeUnique< FUsdListenerImpl >() )
{
}

FUsdListener::FUsdListener( const UE::FUsdStage& Stage )
	: FUsdListener()
{
	Register( Stage );
}

FUsdListener::~FUsdListener() = default;

void FUsdListener::Register( const UE::FUsdStage& Stage )
{
#if USE_USD_SDK
	Impl->Register( Stage );
#endif // #if USE_USD_SDK
}

void FUsdListener::Block()
{
	Impl->IsBlocked.Increment();
}

void FUsdListener::Unblock()
{
	Impl->IsBlocked.Decrement();
}

bool FUsdListener::IsBlocked() const
{
	return Impl->IsBlocked.GetValue() > 0;
}

FUsdListener::FOnStageEditTargetChanged& FUsdListener::GetOnStageEditTargetChanged()
{
	return Impl->OnStageEditTargetChanged;
}

FUsdListener::FOnLayersChanged& FUsdListener::GetOnLayersChanged()
{
	return Impl->OnLayersChanged;
}

FUsdListener::FOnObjectsChanged& FUsdListener::GetOnObjectsChanged()
{
	return Impl->OnObjectsChanged;
}

#if USE_USD_SDK
void FUsdListenerImpl::Register( const pxr::UsdStageRefPtr& Stage )
{
	FScopedUsdAllocs UsdAllocs;

	if ( RegisteredObjectsChangedKey.IsValid() )
	{
		pxr::TfNotice::Revoke( RegisteredObjectsChangedKey );
	}

	RegisteredObjectsChangedKey = pxr::TfNotice::Register( pxr::TfWeakPtr< FUsdListenerImpl >( this ), &FUsdListenerImpl::HandleObjectsChangedNotice, Stage );

	if ( RegisteredStageEditTargetChangedKey.IsValid() )
	{
		pxr::TfNotice::Revoke( RegisteredStageEditTargetChangedKey );
	}

	RegisteredStageEditTargetChangedKey = pxr::TfNotice::Register( pxr::TfWeakPtr< FUsdListenerImpl >( this ), &FUsdListenerImpl::HandleStageEditTargetChangedNotice, Stage );

	if (RegisteredLayersChangedKey.IsValid())
	{
		pxr::TfNotice::Revoke(RegisteredLayersChangedKey);
	}

	RegisteredLayersChangedKey = pxr::TfNotice::Register( pxr::TfWeakPtr< FUsdListenerImpl >( this ), &FUsdListenerImpl::HandleLayersChangedNotice );
	RegisteredLayerDirtinessChangedKey = pxr::TfNotice::Register( pxr::TfWeakPtr< FUsdListenerImpl >( this ), &FUsdListenerImpl::HandleLayerDirtinessChangedNotice );
}
#endif // #if USE_USD_SDK

FUsdListenerImpl::~FUsdListenerImpl()
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;
	pxr::TfNotice::Revoke( RegisteredObjectsChangedKey );
	pxr::TfNotice::Revoke( RegisteredStageEditTargetChangedKey );
	pxr::TfNotice::Revoke( RegisteredLayersChangedKey );
	pxr::TfNotice::Revoke( RegisteredLayerDirtinessChangedKey );
#endif // #if USE_USD_SDK
}

#if USE_USD_SDK
void FUsdListenerImpl::HandleObjectsChangedNotice( const pxr::UsdNotice::ObjectsChanged& Notice, const pxr::UsdStageWeakPtr& Sender )
{
	if ( !OnObjectsChanged.IsBound() )
	{
		return;
	}

	if ( IsBlocked.GetValue() > 0 )
	{
		return;
	}

	UsdUtils::FObjectChangesByPath InfoChanges;
	UsdUtils::FObjectChangesByPath ResyncChanges;
	UsdToUnreal::ConvertObjectsChangedNotice( Notice, InfoChanges, ResyncChanges );
	if ( InfoChanges.Num() > 0 || ResyncChanges.Num() > 0 )
	{
		FScopedUnrealAllocs UnrealAllocs;
		OnObjectsChanged.Broadcast( InfoChanges, ResyncChanges );
	}
}

void FUsdListenerImpl::HandleStageEditTargetChangedNotice( const pxr::UsdNotice::StageEditTargetChanged& Notice, const pxr::UsdStageWeakPtr& Sender )
{
	FScopedUnrealAllocs UnrealAllocs;
	OnStageEditTargetChanged.Broadcast();
}

void FUsdListenerImpl::HandleLayersChangedNotice( const pxr::SdfNotice::LayersDidChange& Notice )
{
	if ( !OnLayersChanged.IsBound() || IsBlocked.GetValue() > 0 )
	{
		return;
	}

	TArray< FString > LayersNames;

	FScopedUsdAllocs UsdAllocs;
	[&](const pxr::SdfLayerChangeListVec& ChangeVec)
	{
		// Check to see if any layer reloaded. If so, rebuild all of our animations as a single layer changing
		// might propagate timecodes through all level sequences
		for (const std::pair<pxr::SdfLayerHandle, pxr::SdfChangeList>& ChangeVecItem : ChangeVec)
		{
			const pxr::SdfChangeList::EntryList& ChangeList = ChangeVecItem.second.GetEntryList();
			for (const std::pair<pxr::SdfPath, pxr::SdfChangeList::Entry>& Change : ChangeList)
			{
				for (const pxr::SdfChangeList::Entry::SubLayerChange& SubLayerChange : Change.second.subLayerChanges)
				{
					const pxr::SdfChangeList::SubLayerChangeType ChangeType = SubLayerChange.second;
					if (ChangeType == pxr::SdfChangeList::SubLayerChangeType::SubLayerAdded ||
						ChangeType == pxr::SdfChangeList::SubLayerChangeType::SubLayerRemoved)
					{
						LayersNames.Add( ANSI_TO_TCHAR( SubLayerChange.first.c_str() ) );
					}
				}

				const pxr::SdfChangeList::Entry::_Flags& Flags = Change.second.flags;
				if (Flags.didReloadContent)
				{
					LayersNames.Add( ANSI_TO_TCHAR( Change.first.GetString().c_str() ) );
				}
			}
		}
	}( Notice.GetChangeListVec() );

	FScopedUnrealAllocs UnrealAllocs;
	OnLayersChanged.Broadcast( LayersNames );
}

void FUsdListenerImpl::HandleLayerDirtinessChangedNotice( const pxr::SdfNotice::LayerDirtinessChanged& Notice )
{
	if ( !OnLayersChanged.IsBound() || IsBlocked.GetValue() > 0 )
	{
		return;
	}

	FScopedUnrealAllocs UnrealAllocs;

	TArray< FString > LayersNames; // We don't know which layer dirtiness has changed unfortunately so we'll provide an empty list for now.
	OnLayersChanged.Broadcast(LayersNames);
}
#endif // #if USE_USD_SDK

FScopedBlockNotices::FScopedBlockNotices( FUsdListener& InListener )
	: Listener( InListener )
{
	Listener.Block();
}

FScopedBlockNotices::~FScopedBlockNotices()
{
	Listener.Unblock();
}
