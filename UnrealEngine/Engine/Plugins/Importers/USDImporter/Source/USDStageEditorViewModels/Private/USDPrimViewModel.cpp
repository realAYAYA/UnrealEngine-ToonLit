// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDPrimViewModel.h"

#include "USDConversionUtils.h"
#include "USDIntegrationUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
	#include "pxr/usd/sdf/path.h"
	#include "pxr/usd/usd/modelAPI.h"
	#include "pxr/usd/usd/payloads.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/tokens.h"
	#include "pxr/usd/usdGeom/xform.h"
	#include "pxr/usd/usdSkel/root.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

#define LOCTEXT_NAMESPACE "USDPrimViewModel"

FUsdPrimViewModel::FUsdPrimViewModel(
	FUsdPrimViewModel* InParentItem,
	const UE::FUsdStageWeak& InUsdStage,
	const UE::FUsdPrim& InPrim
)
	: UsdStage( InUsdStage )
	, UsdPrim( InPrim )
	, ParentItem( InParentItem )
	, RowData( MakeShared< FUsdPrimModel >() )
{
	RefreshData( false );
	FillChildren();
}

TArray< FUsdPrimViewModelRef >& FUsdPrimViewModel::UpdateChildren()
{
	if ( !UsdPrim )
	{
		return Children;
	}

#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	bool bNeedsRefresh = false;

	pxr::UsdPrimSiblingRange PrimChildren = pxr::UsdPrim( UsdPrim ).GetFilteredChildren( pxr::UsdTraverseInstanceProxies( pxr::UsdPrimAllPrimsPredicate ) );

	const int32 NumUsdChildren = (TArray< FUsdPrimViewModelRef >::SizeType )std::distance( PrimChildren.begin(), PrimChildren.end() );
	const int32 NumUnrealChildren = [&]()
	{
		int32 ValidPrims = 0;
		for ( const FUsdPrimViewModelRef& Child : Children )
		{
			if ( !Child->RowData->Name.IsEmpty() )
			{
				++ValidPrims;
			}
		}

		return ValidPrims;
	}();

	if ( NumUsdChildren != NumUnrealChildren )
	{
		FScopedUnrealAllocs UnrealAllocs;

		Children.Reset();
		bNeedsRefresh = true;
	}
	else
	{
		int32 ChildIndex = 0;

		for ( const pxr::UsdPrim& Child : PrimChildren )
		{
			if ( !Children.IsValidIndex( ChildIndex ) || Children[ ChildIndex ]->UsdPrim.GetPrimPath().GetString() != UsdToUnreal::ConvertPath( Child.GetPrimPath() ) )
			{
				FScopedUnrealAllocs UnrealAllocs;

				Children.Reset();
				bNeedsRefresh = true;
				break;
			}

			++ChildIndex;
		}
	}

	if ( bNeedsRefresh )
	{
		FillChildren();
	}
#endif // #if USE_USD_SDK

	return Children;
}

void FUsdPrimViewModel::FillChildren()
{
#if USE_USD_SDK
	if ( !UsdPrim )
	{
		return;
	}

	FScopedUsdAllocs UsdAllocs;
	pxr::UsdPrimSiblingRange PrimChildren = pxr::UsdPrim( UsdPrim ).GetFilteredChildren( pxr::UsdTraverseInstanceProxies( pxr::UsdPrimAllPrimsPredicate ) );

	FScopedUnrealAllocs UnrealAllocs;
	for ( pxr::UsdPrim Child : PrimChildren )
	{
		Children.Add( MakeShared< FUsdPrimViewModel >( this, UsdStage, UE::FUsdPrim( Child ) ) );
	}
#endif // #if USE_USD_SDK
}

void FUsdPrimViewModel::RefreshData( bool bRefreshChildren )
{
#if USE_USD_SDK
	if ( !UsdPrim )
	{
		return;
	}

	const bool bIsPseudoRoot = UsdPrim.GetStage().GetPseudoRoot() == UsdPrim;

	RowData->Name = FText::FromName( bIsPseudoRoot ? TEXT("Stage") : UsdPrim.GetName() );
	RowData->bHasCompositionArcs = UsdUtils::HasCompositionArcs( UsdPrim );

	RowData->Type = bIsPseudoRoot ? FText::GetEmpty() : FText::FromName( UsdPrim.GetTypeName() );
	RowData->bHasPayload = UsdPrim.HasPayload();
	RowData->bIsLoaded = UsdPrim.IsLoaded();

	bool bOldVisibility = RowData->bIsVisible;
	if ( pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable( UsdPrim ) )
	{
		RowData->bIsVisible = ( UsdGeomImageable.ComputeVisibility() != pxr::UsdGeomTokens->invisible );
	}

	// If our visibility was enabled, it may be that the visibilities of all of our parents were enabled to accomplish
	// the target change, so we need to refresh them too. This happens when we manually change visibility on
	// a USceneComponent and write that to the USD Stage, for example
	if ( bOldVisibility == false && RowData->bIsVisible )
	{
		FUsdPrimViewModel* Item = ParentItem;
		while ( Item )
		{
			Item->RefreshData(false);
			Item = Item->ParentItem;
		}
	}

	if ( bRefreshChildren )
	{
		for ( FUsdPrimViewModelRef& Child : UpdateChildren() )
		{
			Child->RefreshData( bRefreshChildren );
		}
	}
#endif // #if USE_USD_SDK
}

bool FUsdPrimViewModel::HasVisibilityAttribute() const
{
#if USE_USD_SDK
	if ( pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable( UsdPrim ) )
	{
		return true;
	}
#endif // #if USE_USD_SDK
	return false;
}

void FUsdPrimViewModel::ToggleVisibility()
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	if ( pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable( UsdPrim ) )
	{
		// MakeInvisible/MakeVisible internally seem to trigger multiple notices, so group them up to prevent some unnecessary updates
		pxr::SdfChangeBlock SdfChangeBlock;

		if ( RowData->IsVisible() )
		{
			UsdGeomImageable.MakeInvisible();
		}
		else
		{
			UsdGeomImageable.MakeVisible();
		}

		RefreshData( false );
	}
#endif // #if USE_USD_SDK
}

void FUsdPrimViewModel::TogglePayload()
{
	if ( UsdPrim && UsdPrim.HasPayload() )
	{
		if ( UsdPrim.IsLoaded() )
		{
			UsdPrim.Unload();
		}
		else
		{
			UsdPrim.Load();
		}

		RefreshData( false );
	}
}

void FUsdPrimViewModel::ApplySchema( FName SchemaName )
{
#if USE_USD_SDK
	UsdUtils::ApplySchema( UsdPrim, UnrealToUsd::ConvertToken( *SchemaName.ToString() ).Get() );
#endif // #if USE_USD_SDK
}

bool FUsdPrimViewModel::CanApplySchema( FName SchemaName ) const
{
#if USE_USD_SDK
	if ( !UsdPrim || UsdPrim.IsPseudoRoot() )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim PxrUsdPrim{ UsdPrim };
	pxr::TfToken SchemaToken = UnrealToUsd::ConvertToken( *SchemaName.ToString() ).Get();

	if ( SchemaToken == UnrealIdentifiers::ControlRigAPI && !PxrUsdPrim.IsA<pxr::UsdSkelRoot>() )
	{
		return false;
	}

	if ( !PxrUsdPrim.IsA<pxr::UsdGeomXformable>() )
	{
		return false;
	}

	return UsdUtils::CanApplySchema( UsdPrim, SchemaToken );
#else
	return false;
#endif // #if USE_USD_SDK
}

void FUsdPrimViewModel::RemoveSchema( FName SchemaName )
{
#if USE_USD_SDK
	UsdUtils::RemoveSchema( UsdPrim, UnrealToUsd::ConvertToken( *SchemaName.ToString() ).Get() );
#endif // #if USE_USD_SDK
}

bool FUsdPrimViewModel::CanRemoveSchema( FName SchemaName ) const
{
#if USE_USD_SDK
	return UsdUtils::CanRemoveSchema( UsdPrim, UnrealToUsd::ConvertToken( *SchemaName.ToString() ).Get() );
#else
	return false;
#endif // #if USE_USD_SDK
}

bool FUsdPrimViewModel::HasSpecsOnLocalLayer() const
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim PxrUsdPrim{ UsdPrim };
	if ( PxrUsdPrim )
	{
		if ( pxr::UsdStageRefPtr PrimUsdStage = PxrUsdPrim.GetStage() )
		{
			for ( const pxr::SdfPrimSpecHandle& Spec : PxrUsdPrim.GetPrimStack() )
			{
				if ( Spec && PrimUsdStage->HasLocalLayer( Spec->GetLayer() ) )
				{
					return true;
				}
			}
		}
	}
#endif // #if USE_USD_SDK

	return false;
}

void FUsdPrimViewModel::DefinePrim( const TCHAR* PrimName )
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	UE::FSdfPath ParentPrimPath;

	if ( ParentItem )
	{
		ParentPrimPath = ParentItem->UsdPrim.GetPrimPath();
	}
	else
	{
		ParentPrimPath = UE::FSdfPath::AbsoluteRootPath();
	}

	UE::FSdfPath NewPrimPath = ParentPrimPath.AppendChild( PrimName );

	UsdPrim = pxr::UsdGeomXform::Define( UsdStage, NewPrimPath ).GetPrim();
#endif // #if USE_USD_SDK
}

void FUsdPrimViewModel::ClearReferences()
{
#if USE_USD_SDK
	if ( !UsdPrim )
	{
		return;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdReferences References = pxr::UsdPrim( UsdPrim ).GetReferences();
	References.ClearReferences();
#endif // #if USE_USD_SDK
}

void FUsdPrimViewModel::ClearPayloads()
{
#if USE_USD_SDK
	if ( !UsdPrim )
	{
		return;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPayloads Payloads = pxr::UsdPrim( UsdPrim ).GetPayloads();
	Payloads.ClearPayloads();
#endif // #if USE_USD_SDK
}

#undef LOCTEXT_NAMESPACE