// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDAttributeUtils.h"

#include "UnrealUSDWrapper.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDProjectSettings.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Notifications/SNotificationList.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
	#include "pxr/usd/pcp/cache.h"
	#include "pxr/usd/pcp/primIndex.h"
	#include "pxr/usd/pcp/propertyIndex.h"
	#include "pxr/usd/usd/usdFileFormat.h"

	#include "pxr/usd/sdf/layer.h"
	#include "pxr/usd/sdf/layerUtils.h"
	#include "pxr/usd/sdf/spec.h"
	#include "pxr/usd/usd/attribute.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/stage.h"
#include "USDIncludesEnd.h"
#endif // #if USE_USD_SDK

#define LOCTEXT_NAMESPACE "UsdAttributeUtils"

namespace UsdUtils
{
#if USE_USD_SDK
	const pxr::TfToken MutedToken = UnrealToUsd::ConvertToken( TEXT( "UE:Muted" ) ).Get();
#endif // #if USE_USD_SDK
}

bool UsdUtils::MuteAttribute( UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage )
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	const pxr::UsdAttribute& UsdAttribute = static_cast< const pxr::UsdAttribute& >( Attribute );
	const pxr::UsdStageRefPtr UsdStage{ Stage };
	if ( !UsdAttribute || !UsdStage )
	{
		return false;
	}

	pxr::SdfLayerRefPtr UEPersistentState = UsdUtils::GetUEPersistentStateSublayer( Stage );
	if ( !UEPersistentState )
	{
		return false;
	}

	pxr::SdfLayerRefPtr UESessionState = UsdUtils::GetUESessionStateSublayer( Stage );
	if ( !UESessionState )
	{
		return false;
	}

	pxr::SdfChangeBlock ChangeBlock;

	// Mark it as muted on the persistent state
	{
		pxr::UsdEditContext Context( Stage, UEPersistentState );

		UsdAttribute.SetCustomDataByKey( MutedToken, pxr::VtValue{ true } );
	}

	// Actually author the opinions that cause it to be muted on the session state
	{
		pxr::UsdEditContext Context( Stage, UESessionState );

		pxr::VtValue Value;
		UsdAttribute.Get( &Value, pxr::UsdTimeCode::Default() );

		// Clear the attribute so that it also gets rid of any time samples it may have
		UsdAttribute.Clear();

		if ( Value.IsEmpty() )
		{
			// It doesn't have any default value, so just mute the attribute completely
			UsdAttribute.Block();
		}
		else
		{
			// It has a default, non-animated value from a weaker opinion: Use that instead
			UsdAttribute.Set( Value );
		}
	}

	return true;
#else
	return false;
#endif // #if USE_USD_SDK
}

bool UsdUtils::UnmuteAttribute( UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage )
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdAttribute& UsdAttribute = static_cast< pxr::UsdAttribute& >( Attribute );
	const pxr::UsdStageRefPtr UsdStage{ Stage };
	if ( !UsdAttribute || !UsdStage )
	{
		return false;
	}

	if ( !IsAttributeMuted( Attribute, Stage ) )
	{
		return true;
	}

	pxr::SdfLayerRefPtr UEPersistentState = UsdUtils::GetUEPersistentStateSublayer( Stage );
	if ( !UEPersistentState )
	{
		return false;
	}

	pxr::SdfLayerRefPtr UESessionState = UsdUtils::GetUESessionStateSublayer( Stage );
	if ( !UESessionState )
	{
		return false;
	}

	pxr::SdfChangeBlock ChangeBlock;

	// Remove the mute tag on the persistent state layer
	{
		pxr::UsdEditContext Context( Stage, UEPersistentState );
		UsdAttribute.ClearCustomDataByKey( MutedToken );
	}

	// Clear our opinion of it on our session state layer
	{
		pxr::UsdEditContext Context( Stage, UESessionState );
		UsdAttribute.Clear();
	}

	return true;
#else
	return false;
#endif // #if USE_USD_SDK
}

bool UsdUtils::IsAttributeMuted( const UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage )
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	const pxr::UsdAttribute& UsdAttribute = static_cast< const pxr::UsdAttribute& >( Attribute );
	if ( !UsdAttribute )
	{
		return false;
	}

	pxr::VtValue Data = UsdAttribute.GetCustomDataByKey( MutedToken );
	if ( Data.IsHolding<bool>() )
	{
		return Data.Get<bool>();
	}
#endif // #if USE_USD_SDK

	return false;
}

void UsdUtils::NotifyIfOverriddenOpinion( const UE::FUsdAttribute& Attribute )
{
#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	pxr::UsdAttribute UsdAttribute{Attribute};
	if ( !UsdAttribute )
	{
		return;
	}

	pxr::UsdStageRefPtr Stage = UsdAttribute.GetPrim().GetStage();
	if ( !Stage )
	{
		return;
	}

	const pxr::UsdEditTarget& EditTarget = Stage->GetEditTarget();
	const pxr::SdfLayerHandle& Layer = EditTarget.GetLayer();
	if ( !Layer )
	{
		return;
	}

	// Currently this will only warn in case our opinion and the strongest one both come from the local layer stack.
	// This is good enough for us at this point though, because we can't edit outside of the local layer stack anyway,
	// which due to LIVRPS is always stronger than the other composition arc types like references, payloads, etc.
	// Alternatively this also means that any other opinion on a non-local composition arc is never going to be strong
	// enough to override anything that we can author.
	// References:
	// - https://graphics.pixar.com/usd/release/glossary.html#livrps-strength-ordering
	// - https://groups.google.com/g/usd-interest/c/xTxFYQA_bRs/m/qbGkvx3yAgAJ
	std::vector<pxr::SdfPropertySpecHandle> SpecStack = UsdAttribute.GetPropertyStack();
	for ( const pxr::SdfPropertySpecHandle& Spec : SpecStack )
	{
		if ( !Spec )
		{
			continue;
		}

		const pxr::SdfLayerHandle& SpecLayer = Spec->GetLayer();
		if ( SpecLayer != Layer )
		{
			const FText Text = LOCTEXT( "OverridenOpinionText", "USD: Overridden opinion" );

			const FText SubText = FText::Format(
				LOCTEXT( "OverridenOpinionSubText", "Opinion authored for this attribute:\n\n{0}\n\nAt this layer:\n\n{1}\n\nIs overridden by another spec at this layer:\n\n{2}\n\nAnd so may not be visible on the composed stage. This means this edit may not be visible once the stage is reloaded." ),
				FText::FromString( UsdToUnreal::ConvertPath( Spec->GetPath() ) ),
				FText::FromString( UsdToUnreal::ConvertString( Layer->GetIdentifier() ) ),
				FText::FromString( UsdToUnreal::ConvertString( SpecLayer->GetIdentifier() ) )
			);

			UE_LOG( LogUsd, Warning, TEXT( "%s" ), *SubText.ToString().Replace( TEXT( "\n\n" ), TEXT( " " ) ) );

			const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
			if ( Settings && Settings->bShowOverriddenOpinionsWarning )
			{
				static TWeakPtr<SNotificationItem> Notification;

				FNotificationInfo Toast( Text );
				Toast.SubText = SubText;
				Toast.Image = FCoreStyle::Get().GetBrush( TEXT( "MessageLog.Warning" ) );
				Toast.CheckBoxText = LOCTEXT("DontAskAgain", "Don't prompt again");
				Toast.bUseLargeFont = false;
				Toast.bFireAndForget = false;
				Toast.FadeOutDuration = 0.0f;
				Toast.ExpireDuration = 0.0f;
				Toast.bUseThrobber = false;
				Toast.bUseSuccessFailIcons = false;
				Toast.ButtonDetails.Emplace(
					LOCTEXT( "OverridenOpinionMessageOk", "Ok" ),
					FText::GetEmpty(),
					FSimpleDelegate::CreateLambda([](){
						if ( TSharedPtr<SNotificationItem> PinnedNotification = Notification.Pin() )
						{
							PinnedNotification->SetCompletionState( SNotificationItem::CS_Success );
							PinnedNotification->ExpireAndFadeout();
						}
					})
				);
				// This is flipped because the default checkbox message is "Don't prompt again"
				Toast.CheckBoxState = Settings->bShowOverriddenOpinionsWarning ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
				Toast.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic([](ECheckBoxState NewState)
				{
					if ( UUsdProjectSettings* Settings = GetMutableDefault<UUsdProjectSettings>() )
					{
						// This is flipped because the default checkbox message is "Don't prompt again"
						Settings->bShowOverriddenOpinionsWarning = NewState == ECheckBoxState::Unchecked;
						Settings->SaveConfig();
					}
				});

				// Only show one at a time
				if ( !Notification.IsValid() )
				{
					Notification = FSlateNotificationManager::Get().AddNotification( Toast );
				}

				if ( TSharedPtr<SNotificationItem> PinnedNotification = Notification.Pin() )
				{
					PinnedNotification->SetCompletionState( SNotificationItem::CS_Pending );
				}
			}
		}
		else
		{
			break;
		}
	}
#endif // #if USE_USD_SDK
}

#undef LOCTEXT_NAMESPACE