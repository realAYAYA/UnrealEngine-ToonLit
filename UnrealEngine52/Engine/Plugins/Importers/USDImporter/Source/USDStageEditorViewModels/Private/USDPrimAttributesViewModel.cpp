// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDPrimAttributesViewModel.h"

#include "UnrealUSDWrapper.h"
#include "USDAttributeUtils.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/VtValue.h"

#include "Framework/Notifications/NotificationManager.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/base/tf/stringUtils.h"
	#include "pxr/base/vt/value.h"
	#include "pxr/usd/kind/registry.h"
	#include "pxr/usd/sdf/types.h"
	#include "pxr/usd/usd/attribute.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usdGeom/tokens.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

#define LOCTEXT_NAMESPACE "USDPrimAttributesViewModel"

FUsdPrimAttributeViewModel::FUsdPrimAttributeViewModel( FUsdPrimAttributesViewModel* InOwner )
	: Owner( InOwner )
{
}

TArray< TSharedPtr< FString > > FUsdPrimAttributeViewModel::GetDropdownOptions() const
{
#if USE_USD_SDK
	if ( Label == TEXT("kind") )
	{
		TArray< TSharedPtr< FString > > Options;
		{
			FScopedUsdAllocs Allocs;

			std::vector< pxr::TfToken > Kinds = pxr::KindRegistry::GetAllKinds();
			Options.Reserve( Kinds.size() );

			for ( const pxr::TfToken& Kind : Kinds )
			{
				Options.Add( MakeShared< FString >( UsdToUnreal::ConvertToken( Kind ) ) );
			}

			// They are supposed to be in an unspecified order, so let's make them consistent
			Options.Sort( [](const TSharedPtr< FString >& A, const TSharedPtr< FString >& B )
				{
					return A.IsValid() && B.IsValid() && ( *A < *B );
				}
			);

		}
		return Options;
	}
	else if ( Label == UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->purpose ) )
	{
		return TArray< TSharedPtr< FString > >
		{
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->default_ ) ),
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->proxy ) ),
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->render) ),
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->guide ) ),
		};
	}
	else if ( Label == UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->upAxis ) )
	{
		return TArray< TSharedPtr< FString > >
		{
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->y ) ),
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->z ) ),
		};
	}
	else if ( Label == UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->visibility ) )
	{
		return TArray< TSharedPtr< FString > >
		{
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->inherited ) ),
			MakeShared< FString >( UsdToUnreal::ConvertToken( pxr::UsdGeomTokens->invisible ) ),
		};
	}
#endif // #if USE_USD_SDK

	return {};
}

void FUsdPrimAttributeViewModel::SetAttributeValue( const UsdUtils::FConvertedVtValue& InValue )
{
	Owner->SetPrimAttribute( Label, InValue );
}

template<typename T>
void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString& AttributeName, const T& Value, UsdUtils::EUsdBasicDataTypes SourceType, const FString& ValueRole, bool bReadOnly )
{
	UsdUtils::FConvertedVtValue VtValue;
	VtValue.Entries = { { UsdUtils::FConvertedVtValueComponent{ TInPlaceType<T>(), Value } } };
	VtValue.SourceType = SourceType;

	FUsdPrimAttributeViewModel Property( this );
	Property.Label = AttributeName;
	Property.Value = VtValue;
	Property.ValueRole = ValueRole;
	Property.bReadOnly = bReadOnly;

	PrimAttributes.Add( MakeSharedUnreal< FUsdPrimAttributeViewModel >( MoveTemp( Property ) ) );
}

void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString& AttributeName, const UsdUtils::FConvertedVtValue& Value, bool bReadOnly )
{
	FUsdPrimAttributeViewModel Property( this );
	Property.Label = AttributeName;
	Property.Value = Value;
	Property.bReadOnly = bReadOnly;

	PrimAttributes.Add( MakeSharedUnreal< FUsdPrimAttributeViewModel >( MoveTemp( Property ) ) );
}

void FUsdPrimAttributesViewModel::SetPrimAttribute( const FString& AttributeName, const UsdUtils::FConvertedVtValue& InValue )
{
	bool bSuccess = false;

#if USE_USD_SDK
	if ( !UsdStage )
	{
		return;
	}

	// Transact here as setting this attribute may trigger USD events that affect assets/components
	FScopedTransaction Transaction( FText::Format(
		LOCTEXT( "SetPrimAttribute", "Set value for attribute '{0}' of prim '{1}'" ),
		FText::FromString( AttributeName ),
		FText::FromString( PrimPath )
	));

	const bool bIsStageAttribute = PrimPath == TEXT( "/" ) || PrimPath.IsEmpty();

	UE::FVtValue VtValue;
	if ( UnrealToUsd::ConvertValue( InValue, VtValue ) )
	{
		if ( bIsStageAttribute )
		{
			FScopedUsdAllocs UsdAllocs;

			// To set stage metadata the edit target must be the root or session layer
			pxr::UsdStageRefPtr Stage{ UsdStage };
			pxr::UsdEditContext( Stage, Stage->GetRootLayer() );
			bSuccess = UsdStage.SetMetadata( *AttributeName, VtValue );
		}
		else if ( UE::FUsdPrim UsdPrim = UsdStage.GetPrimAtPath( UE::FSdfPath( *PrimPath ) ) )
		{
			// Single value, single component of FString
			if ( AttributeName == TEXT( "kind" ) && InValue.Entries.Num() == 1 && InValue.Entries[ 0 ].Num() == 1 && InValue.Entries[ 0 ][ 0 ].IsType<FString>() )
			{
				bSuccess = IUsdPrim::SetKind(
					UsdPrim,
					UnrealToUsd::ConvertToken( *( InValue.Entries[ 0 ][ 0 ].Get<FString>() ) ).Get()
				);
			}
			else if ( UE::FUsdAttribute Attribute = UsdPrim.GetAttribute( *AttributeName ) )
			{
				bSuccess = Attribute.Set( VtValue );
				UsdUtils::NotifyIfOverriddenOpinion( Attribute );
			}
		}
	}

	if ( !bSuccess )
	{
		const FText ErrorMessage = FText::Format( LOCTEXT( "FailToSetAttributeMessage", "Failed to set attribute '{0}' on {1} '{2}'" ),
			FText::FromString( AttributeName ),
			FText::FromString( bIsStageAttribute ? TEXT( "stage" ) : TEXT( "prim" ) ),
			FText::FromString( bIsStageAttribute ? UsdStage.GetRootLayer().GetRealPath() : PrimPath )
		);

		FNotificationInfo ErrorToast( ErrorMessage );
		ErrorToast.ExpireDuration = 5.0f;
		ErrorToast.bFireAndForget = true;
		ErrorToast.Image = FCoreStyle::Get().GetBrush( TEXT( "MessageLog.Warning" ) );
		FSlateNotificationManager::Get().AddNotification( ErrorToast );

		FUsdLogManager::LogMessage( EMessageSeverity::Warning, ErrorMessage );
	}
#endif // #if USE_USD_SDK
}

void FUsdPrimAttributesViewModel::Refresh( const UE::FUsdStageWeak& InUsdStage, const TCHAR* InPrimPath, float TimeCode )
{
	FScopedUnrealAllocs UnrealAllocs;

	UsdStage = InUsdStage;
	PrimPath = InPrimPath;

	PrimAttributes.Reset();

#if USE_USD_SDK
	if ( UsdStage )
	{
		// Show info about the stage
		if ( PrimPath.Equals( TEXT( "/" ) ) || PrimPath.IsEmpty() )
		{
			const bool bReadOnly = true;
			const FString Role = TEXT( "" );
			CreatePrimAttribute( TEXT( "path" ), UsdStage.GetRootLayer().GetRealPath(), UsdUtils::EUsdBasicDataTypes::String, Role, bReadOnly );

			FScopedUsdAllocs UsdAllocs;

			std::vector<pxr::TfToken> TokenVector = pxr::SdfSchema::GetInstance().GetMetadataFields( pxr::SdfSpecType::SdfSpecTypePseudoRoot );
			for ( const pxr::TfToken& Token : TokenVector )
			{
				pxr::VtValue VtValue;
				if ( pxr::UsdStageRefPtr( UsdStage )->GetMetadata( Token, &VtValue ) )
				{
					FString AttributeName = UsdToUnreal::ConvertToken( Token );

					UsdUtils::FConvertedVtValue ConvertedValue;
					if ( !UsdToUnreal::ConvertValue( UE::FVtValue{ VtValue }, ConvertedValue ) )
					{
						continue;
					}

					const bool bAttrReadOnly = ConvertedValue.bIsArrayValued;
					CreatePrimAttribute( AttributeName, ConvertedValue, bAttrReadOnly );
				}
			}

		}
		// Show info about a prim
		else if ( UE::FUsdPrim UsdPrim = UsdStage.GetPrimAtPath( UE::FSdfPath( InPrimPath ) ) )
		{
			// For now we can't rename/reparent prims through this
			const bool bReadOnly = true;
			const FString Role = TEXT( "" );
			CreatePrimAttribute( TEXT( "name" ), UsdPrim.GetName().ToString(), UsdUtils::EUsdBasicDataTypes::String, Role, bReadOnly );
			CreatePrimAttribute( TEXT( "path" ), FString( InPrimPath ), UsdUtils::EUsdBasicDataTypes::String, Role, bReadOnly );
			CreatePrimAttribute( TEXT( "kind" ), UsdToUnreal::ConvertString( IUsdPrim::GetKind( UsdPrim ).GetString() ), UsdUtils::EUsdBasicDataTypes::Token );

			FScopedUsdAllocs UsdAllocs;

			for ( const pxr::UsdAttribute& PrimAttribute : pxr::UsdPrim( UsdPrim ).GetAttributes() )
			{
				FString AttributeName = UsdToUnreal::ConvertString( PrimAttribute.GetName().GetString() );

				pxr::VtValue VtValue;
				PrimAttribute.Get( &VtValue, TimeCode );

				// Just show arrays as readonly strings for now
				if ( VtValue.IsArrayValued() )
				{
					const bool bAttrReadOnly = true;
					FString Stringified = UsdToUnreal::ConvertString( pxr::TfStringify( VtValue ) );

					// STextBlock can get very slow calculating its desired size for very long string so chop it if needed
					const int32 MaxValueLength = 300;
					if ( Stringified.Len() > MaxValueLength )
					{
						Stringified.LeftInline( MaxValueLength );
						Stringified.Append( TEXT( "..." ) );
					}

					CreatePrimAttribute( AttributeName, Stringified, UsdUtils::EUsdBasicDataTypes::String, TEXT( "" ), bAttrReadOnly );
				}
				else
				{
					UsdUtils::FConvertedVtValue ConvertedValue;
					if ( UsdToUnreal::ConvertValue( UE::FVtValue{ VtValue }, ConvertedValue ) )
					{
						const bool bAttrReadOnly = false;
						CreatePrimAttribute( AttributeName, ConvertedValue, bAttrReadOnly );
					}

					if ( PrimAttribute.HasAuthoredConnections() )
					{
						const FString ConnectionAttributeName = AttributeName + TEXT(":connect");

						pxr::SdfPathVector ConnectedSources;
						PrimAttribute.GetConnections( &ConnectedSources );

						for ( pxr::SdfPath& ConnectedSource : ConnectedSources )
						{
							UsdUtils::FConvertedVtValueEntry Entry;
							Entry.Emplace( TInPlaceType< FString >(), UsdToUnreal::ConvertPath( ConnectedSource ) );

							UsdUtils::FConvertedVtValue ConnectionPropertyValue;
							ConnectionPropertyValue.SourceType = UsdUtils::EUsdBasicDataTypes::String;
							ConnectionPropertyValue.Entries = { Entry };

							const bool bConnectionValueReadOnly = true;
							CreatePrimAttribute( ConnectionAttributeName, ConnectionPropertyValue, bConnectionValueReadOnly );
						}
					}
				}
			}
		}
	}

	PrimAttributes.Sort([](const TSharedPtr<FUsdPrimAttributeViewModel>& A, const TSharedPtr<FUsdPrimAttributeViewModel>& B)
	{
		return A->Label < B->Label;
	});
#endif // #if USE_USD_SDK
}

// One for each variant of FPrimPropertyValue. These should all be implicitly instantiated from the above code anyway, but just in case that changes somehow
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const bool&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const uint8&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const int32&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const uint32&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const int64&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const uint64&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const float&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const double&,	UsdUtils::EUsdBasicDataTypes, const FString&, bool );
template void FUsdPrimAttributesViewModel::CreatePrimAttribute( const FString&, const FString&, UsdUtils::EUsdBasicDataTypes, const FString&, bool );

#undef LOCTEXT_NAMESPACE


