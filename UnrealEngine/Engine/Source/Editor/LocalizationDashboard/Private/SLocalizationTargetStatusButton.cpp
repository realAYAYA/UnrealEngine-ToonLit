// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLocalizationTargetStatusButton.h"

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationTargetTypes.h"
#include "Math/Color.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;

#define LOCTEXT_NAMESPACE "LocalizationTargetStatusButton"

void SLocalizationTargetStatusButton::Construct(const FArguments& InArgs, ULocalizationTarget& InTarget)
{
	Target = &InTarget;

	SButton::Construct(
		SButton::FArguments()
		.ButtonStyle( FAppStyle::Get(), TEXT("HoverHintOnly") )
		.OnClicked(this, &SLocalizationTargetStatusButton::OnClicked)
		);

		ChildSlot
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SLocalizationTargetStatusButton::GetImageBrush)
				.ColorAndOpacity(this, &SLocalizationTargetStatusButton::GetColorAndOpacity)
				.ToolTipText(this, &SLocalizationTargetStatusButton::GetToolTipText)
			];
}

const FSlateBrush* SLocalizationTargetStatusButton::GetImageBrush() const
{
	switch (Target->Settings.ConflictStatus)
	{
	default:
	case ELocalizationTargetConflictStatus::Unknown:
		return FAppStyle::GetBrush("Icons.Warning");
		break;
	case ELocalizationTargetConflictStatus::Clear:
		return FAppStyle::GetBrush("Symbols.Check");
		break;
	case ELocalizationTargetConflictStatus::ConflictsPresent:
		return FAppStyle::GetBrush("Symbols.X");
		break;
	}
}

FSlateColor SLocalizationTargetStatusButton::GetColorAndOpacity() const
{
	switch (Target->Settings.ConflictStatus)
	{
	default:
	case ELocalizationTargetConflictStatus::Unknown:
		return FLinearColor::White;
		break;
	case ELocalizationTargetConflictStatus::Clear:
		return FLinearColor::Green;
		break;
	case ELocalizationTargetConflictStatus::ConflictsPresent:
		return FLinearColor::Red;
		break;
	}
}

FText SLocalizationTargetStatusButton::GetToolTipText() const
{
	switch (Target->Settings.ConflictStatus)
	{
	default:
	case ELocalizationTargetConflictStatus::Unknown:
		return LOCTEXT("StatusToolTip_Unknown", "Conflict report file not detected. Perform a gather to generate a conflict report file.");
		break;
	case ELocalizationTargetConflictStatus::Clear:
		return LOCTEXT("StatusToolTip_Clear", "No conflicts detected.");
		break;
	case ELocalizationTargetConflictStatus::ConflictsPresent:
		return LOCTEXT("StatusToolTip_ConflictsPresent", "Conflicts detected. Click to open the conflict report.");
		break;
	}
}

FReply SLocalizationTargetStatusButton::OnClicked()
{
	switch (Target->Settings.ConflictStatus)
	{
	case ELocalizationTargetConflictStatus::Unknown:
		// Do nothing.
		break;
	case ELocalizationTargetConflictStatus::Clear:
		// Do nothing.
		break;
	case ELocalizationTargetConflictStatus::ConflictsPresent:
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*FPaths::ConvertRelativePathToFull(LocalizationConfigurationScript::GetConflictReportPath(Target)), nullptr, ELaunchVerb::Open);
		break;
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
