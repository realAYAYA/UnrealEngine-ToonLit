// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILauncherProfile.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherValidation"


/**
 * Implements the launcher's profile validation panel.
 */
class SProjectLauncherValidation
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherValidation) { }
		SLATE_ATTRIBUTE(TSharedPtr<ILauncherProfile>, LaunchProfile)
	SLATE_END_ARGS()


public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct( const FArguments& InArgs)
	{
		LaunchProfileAttr = InArgs._LaunchProfile;

		TSharedPtr<SVerticalBox> VertBox;
		SAssignNew(VertBox, SVerticalBox);

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CopyToDeviceRequiresCookByTheBookError", "Deployment by copying to device requires 'By The Book' cooking.").ToString(), ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CustomRolesNotSupportedYet", "Custom launch roles are not supported yet.").ToString(), ELauncherProfileValidationErrors::CustomRolesNotSupportedYet)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("DeployedDeviceGroupRequired", "A device group must be selected when deploying builds.").ToString(), ELauncherProfileValidationErrors::DeployedDeviceGroupRequired)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialCultureNotAvailable", "The Initial Culture selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialCultureNotAvailable)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialMapNotAvailable", "The Initial Map selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialMapNotAvailable)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("MalformedLaunchCommandLine", "The specified launch command line is not formatted correctly.").ToString(), ELauncherProfileValidationErrors::MalformedLaunchCommandLine)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildConfigurationSelectedError", "A Build Configuration must be selected.").ToString(), ELauncherProfileValidationErrors::NoBuildConfigurationSelected)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedCulturesSelectedError", "At least one Culture must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoCookedCulturesSelected)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoLaunchRoleDeviceAssigned", "One or more launch roles do not have a device assigned.").ToString(), ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoPlatformSelected)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildGameSelectedError", "A Project must be selected.").ToString(), ELauncherProfileValidationErrors::NoProjectSelected)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoPackageDirectorySpecified", "The deployment requires a package directory to be specified.").ToString(), ELauncherProfileValidationErrors::NoPackageDirectorySpecified)
			];

		VertBox->AddSlot().AutoHeight()
			[
			MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("LaunchDeviceIsUnauthorized", "Device is unauthorized or locked.").ToString(), ELauncherProfileValidationErrors::LaunchDeviceIsUnauthorized)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeCallbackMessage(TEXT("Icons.Error"), ELauncherProfileValidationErrors::NoPlatformSDKInstalled)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("UnversionedAndIncrimental", "Unversioned build cannot be incremental.").ToString(), ELauncherProfileValidationErrors::UnversionedAndIncrimental)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingPatchesCanOnlyRunFromByTheBookCookMode", "Generating patch requires cook by the book mode.").ToString(), ELauncherProfileValidationErrors::GeneratingPatchesCanOnlyRunFromByTheBookCookMode)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingMultiLevelPatchesRequiresGeneratePatch", "Generating multilevel patch requires generating patch.").ToString(), ELauncherProfileValidationErrors::GeneratingMultiLevelPatchesRequiresGeneratePatch)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("StagingBaseReleasePaksWithoutABaseReleaseVersion", "Staging base release pak files requires a base release version to be specified").ToString(), ELauncherProfileValidationErrors::StagingBaseReleasePaksWithoutABaseReleaseVersion)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingChunksRequiresCookByTheBook", "Generating Chunks requires cook by the book mode.").ToString(), ELauncherProfileValidationErrors::GeneratingChunksRequiresCookByTheBook)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingChunksRequiresUnrealPak", "UnrealPak must be selected to Generate Chunks.").ToString(), ELauncherProfileValidationErrors::GeneratingChunksRequiresUnrealPak)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingHttpChunkDataRequiresGeneratingChunks", "Generate Chunks must be selected to Generate Http Chunk Install Data.").ToString(), ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresGeneratingChunks)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingHttpChunkDataRequiresValidDirectoryAndName", "Generating Http Chunk Install Data requires a valid directory and release name.").ToString(), ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresValidDirectoryAndName)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly", "Shipping doesn't support commandline options and can't use cook on the fly").ToString(), ELauncherProfileValidationErrors::ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CookOnTheFlyDoesntSupportServer", "Cook on the fly doesn't support server target configurations").ToString(), ELauncherProfileValidationErrors::CookOnTheFlyDoesntSupportServer)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoArchiveDirectorySpecifiedError", "The archive step requires a valid directory.").ToString(), ELauncherProfileValidationErrors::NoArchiveDirectorySpecified)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("IoStoreRequiresPakFilesError", "UnrealPak must be selected when using I/O store container file(s)").ToString(), ELauncherProfileValidationErrors::IoStoreRequiresPakFiles)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("BuildTargetCookVariantMismatch", "Build Target and Cook Variant mismatch.").ToString(), ELauncherProfileValidationErrors::BuildTargetCookVariantMismatch)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("BuildTargetIsRequired", "This profile requires an explicit Build Target set.").ToString(), ELauncherProfileValidationErrors::BuildTargetIsRequired)
			];

		VertBox->AddSlot().AutoHeight()
			[
				MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("FallbackBuildTargetIsRequired", "An explicit Default Build Target is required for the selected Variant.").ToString(), ELauncherProfileValidationErrors::FallbackBuildTargetIsRequired)
			];

		check(VertBox->NumSlots() == ELauncherProfileValidationErrors::Count);

		ChildSlot[VertBox.ToSharedRef()];
	}

protected:

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageText The message text.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeValidationMessage( const TCHAR* IconName, const FString& MessageText, ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FAppStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(MessageText))
			];
	}

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeCallbackMessage( const TCHAR* IconName,ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FAppStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(this, &SProjectLauncherValidation::HandleValidationMessage, Message)
			];
	}

private:

	// Callback for getting the visibility state of a validation message.
	EVisibility HandleValidationMessageVisibility( ELauncherProfileValidationErrors::Type Error ) const
	{
		TSharedPtr<ILauncherProfile> LaunchProfile = LaunchProfileAttr.Get();
		if (!LaunchProfile.IsValid() || LaunchProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	FText HandleValidationMessage( ELauncherProfileValidationErrors::Type Error ) const
	{
		TSharedPtr<ILauncherProfile> LaunchProfile = LaunchProfileAttr.Get();
		if (LaunchProfile.IsValid())
		{
			if (LaunchProfile->HasValidationError(Error))
			{
				return FText::Format(LOCTEXT("NoPlatformSDKInstalledFmt", "A required platform SDK is missing: {0}"), FText::FromString(LaunchProfile->GetInvalidPlatform()));
			}

			return FText::GetEmpty();
		}
		return LOCTEXT("InvalidLaunchProfile", "Invalid Launch Profile.");
	}

private:

	// Attribute for the launch profile this widget shows validation for. 
	TAttribute<TSharedPtr<ILauncherProfile>> LaunchProfileAttr;
};


#undef LOCTEXT_NAMESPACE
