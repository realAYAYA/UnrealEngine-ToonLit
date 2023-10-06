// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ForceFeedbackEffect.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "GenericPlatform/IInputInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_ForceFeedbackEffect::GetSupportedClass() const
{
	return UForceFeedbackEffect::StaticClass();
}

void FAssetTypeActions_ForceFeedbackEffect::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UForceFeedbackEffect>> Effects = GetTypedWeakObjectPtrs<UForceFeedbackEffect>(InObjects);

	Section.AddMenuEntry(
		"ForceFeedbackEffect_PlayEffect",
		LOCTEXT("ForceFeedbackEffect_PlayEffect", "Play"),
		LOCTEXT("ForceFeedbackEffect_PlayEffectTooltip", "Plays the selected force feedback effect."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Play.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::ExecutePlayEffect, Effects ),
			FCanExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::CanExecutePlayCommand, Effects )
			)
		);

	Section.AddMenuEntry(
		"ForceFeedbackEffect_StopEffect",
		LOCTEXT("ForceFeedbackEffect_StopEffect", "Stop"),
		LOCTEXT("ForceFeedbackEffect_StopEffectTooltip", "Stops the selected force feedback effect."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Stop.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::ExecuteStopEffect, Effects ),
			FCanExecuteAction()
			)
		);
}

bool FAssetTypeActions_ForceFeedbackEffect::IsEffectPlaying(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects) const
{
	if (PreviewForceFeedbackEffect.ForceFeedbackEffect)
	{
		for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
		{
			UForceFeedbackEffect* Effect = EffectPtr.Get();
			if (Effect && PreviewForceFeedbackEffect.ForceFeedbackEffect == Effect)
			{
				return true;
			}
		}
	}

	return false;
}

bool FAssetTypeActions_ForceFeedbackEffect::IsEffectPlaying(const UForceFeedbackEffect* ForceFeedbackEffect) const
{
	return PreviewForceFeedbackEffect.ForceFeedbackEffect && PreviewForceFeedbackEffect.ForceFeedbackEffect == ForceFeedbackEffect;
}

bool FAssetTypeActions_ForceFeedbackEffect::IsEffectPlaying(const FAssetData& AssetData) const
{
	if (PreviewForceFeedbackEffect.ForceFeedbackEffect)
	{
		if (PreviewForceFeedbackEffect.ForceFeedbackEffect->GetFName() == AssetData.AssetName)
		{
			if (PreviewForceFeedbackEffect.ForceFeedbackEffect->GetOutermost()->GetFName() == AssetData.PackageName)
			{
				return true;
			}
		}
	}

	return false;
}

bool FAssetTypeActions_ForceFeedbackEffect::CanExecutePlayCommand(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects) const
{
	return Objects.Num() == 1;
}

bool FAssetTypeActions_ForceFeedbackEffect::AssetsActivatedOverride( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		for (UObject* Object : InObjects)
		{
			UForceFeedbackEffect* TargetEffect = Cast<UForceFeedbackEffect>(Object);
			if ( TargetEffect )
			{
				// Only target the first valid effect
				TArray<TWeakObjectPtr<UForceFeedbackEffect>> EffectList;
				EffectList.Add(MakeWeakObjectPtr(TargetEffect));
				if (IsEffectPlaying(EffectList))
				{
					ExecuteStopEffect(EffectList);
				}
				else
				{
					ExecutePlayEffect(EffectList);
				}

				return true;
			}
		}
	}
	return false;
}

void FAssetTypeActions_ForceFeedbackEffect::ExecutePlayEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects)
{
	for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
	{
		UForceFeedbackEffect* Effect = EffectPtr.Get();
		if (Effect)
		{
			// Only play the first valid effect
			PlayEffect(Effect);
			break;
		}
	}
}

void FAssetTypeActions_ForceFeedbackEffect::ExecuteStopEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects)
{
	StopEffect();
}

void FAssetTypeActions_ForceFeedbackEffect::PlayEffect(UForceFeedbackEffect* Effect)
{
	if (Effect)
	{
		PreviewForceFeedbackEffect.ForceFeedbackEffect = Effect;
		PreviewForceFeedbackEffect.PlayTime = 0.f;
		PreviewForceFeedbackEffect.PlatformUser = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
		PreviewForceFeedbackEffect.ActivateDeviceProperties();
	}
	else
	{
		StopEffect();
	}
}

void FAssetTypeActions_ForceFeedbackEffect::StopEffect() 
{
	PreviewForceFeedbackEffect.ResetDeviceProperties();
	PreviewForceFeedbackEffect.ForceFeedbackEffect = nullptr;

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		InputInterface->SetForceFeedbackChannelValues(0, FForceFeedbackValues());
	}
}

TSharedPtr<SWidget> FAssetTypeActions_ForceFeedbackEffect::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	auto OnGetDisplayBrushLambda = [this, AssetData]() -> const FSlateBrush*
	{
		if (IsEffectPlaying(AssetData))
		{
			return FAppStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
		}

		return FAppStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	FAssetTypeActions_ForceFeedbackEffect* MutableThis = const_cast<FAssetTypeActions_ForceFeedbackEffect*>(this);
	auto OnClickedLambda = [MutableThis, AssetData]() -> FReply
	{
		if (MutableThis->IsEffectPlaying(AssetData))
		{
			MutableThis->StopEffect();
		}
		else
		{
			// Load and play asset
			MutableThis->PlayEffect(Cast<UForceFeedbackEffect>(AssetData.GetAsset()));
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, AssetData]() -> FText
	{
		if (IsEffectPlaying(AssetData))
		{
			return LOCTEXT("Thumbnail_StopForceFeedbackToolTip", "Stop selected force feedback effect");
		}

		return LOCTEXT("Thumbnail_PlayForceFeedbackToolTip", "Play selected force feedback effect");
	};

	TSharedRef<SBox> Box = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box, AssetData]() -> EVisibility
	{
		if (Box->IsHovered() || IsEffectPlaying(AssetData))
		{
			return EVisibility::Visible;
		}

		return EVisibility::Hidden;
	};

	TSharedRef<SButton> BoxContent = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so we need to override that here
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		.OnClicked_Lambda(OnClickedLambda)
		.Visibility_Lambda(OnGetVisibilityLambda)
		[
			SNew(SBox)
			.MinDesiredWidth(16.f)
			.MinDesiredHeight(16.f)
			[
				SNew(SImage)
				.Image_Lambda(OnGetDisplayBrushLambda)
			]
		];

	Box->SetContent(BoxContent);
	Box->SetVisibility(EVisibility::Visible);

	return Box;
}

bool FPreviewForceFeedbackEffect::IsTickable() const
{
	return (ForceFeedbackEffect != nullptr);
}

void FPreviewForceFeedbackEffect::Tick( float DeltaTime )
{
	FForceFeedbackValues ForceFeedbackValues;

	if (!Update(DeltaTime, ForceFeedbackValues))
	{
		ResetDeviceProperties();
		ForceFeedbackEffect = nullptr;
	}

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		InputInterface->SetForceFeedbackChannelValues(0, ForceFeedbackValues);
	}
}

TStatId FPreviewForceFeedbackEffect::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPreviewForceFeedbackEffect, STATGROUP_Tickables);
}

void FPreviewForceFeedbackEffect::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(ForceFeedbackEffect);
}

#undef LOCTEXT_NAMESPACE
