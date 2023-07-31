// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDReferencesViewModel.h"

#include "USDMemory.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/sdf/reference.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/primCompositionQuery.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

void FUsdReferencesViewModel::UpdateReferences( const UE::FUsdStageWeak& UsdStage, const TCHAR* PrimPath )
{
	References.Reset();

	if ( !UsdStage || !PrimPath || FString{ PrimPath }.IsEmpty() || UE::FSdfPath{ PrimPath }.IsAbsoluteRootPath() )
	{
		return;
	}

#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	if ( pxr::UsdPrim Prim{ UsdStage.GetPrimAtPath( UE::FSdfPath( PrimPath ) ) } )
	{
		pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery::GetDirectReferences( Prim );

		for ( const pxr::UsdPrimCompositionQueryArc& CompositionArc : PrimCompositionQuery.GetCompositionArcs() )
		{
			if ( CompositionArc.GetArcType() == pxr::PcpArcTypeReference )
			{
				pxr::SdfReferenceEditorProxy ReferenceEditor;
				pxr::SdfReference UsdReference;

				if ( CompositionArc.GetIntroducingListEditor( &ReferenceEditor, &UsdReference ) )
				{
					FUsdReference Reference;
					Reference.AssetPath = UsdToUnreal::ConvertString( UsdReference.GetAssetPath() );

					References.Add( MakeSharedUnreal< FUsdReference >( MoveTemp( Reference ) ) );
				}
			}
		}
	}
#endif // #if USE_USD_SDK
}