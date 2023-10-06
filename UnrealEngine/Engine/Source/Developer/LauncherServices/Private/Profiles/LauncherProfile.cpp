// Copyright Epic Games, Inc. All Rights Reserved.

#include "LauncherProfile.h"

#define LOCTEXT_NAMESPACE "SProjectLauncherValidation"

FString LexToStringLocalized(ELauncherProfileValidationErrors::Type Value)
{
	static_assert(ELauncherProfileValidationErrors::Count == 30, "GetLocalizedValidationErrorMessage() needs to be updated to account for modified enum values.");
	switch (Value)
	{
		case ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook:
			return LOCTEXT("CopyToDeviceRequiresCookByTheBookError", "Deployment by copying to device requires 'By The Book' cooking.").ToString();
		case ELauncherProfileValidationErrors::CustomRolesNotSupportedYet:
			return LOCTEXT("CustomRolesNotSupportedYet", "Custom launch roles are not supported yet.").ToString();
		case ELauncherProfileValidationErrors::DeployedDeviceGroupRequired:
			return LOCTEXT("DeployedDeviceGroupRequired", "A device group must be selected when deploying builds.").ToString();
		case ELauncherProfileValidationErrors::InitialCultureNotAvailable:
			return LOCTEXT("InitialCultureNotAvailable", "The Initial Culture selected for launch is not in the build.").ToString();
		case ELauncherProfileValidationErrors::InitialMapNotAvailable:
			return LOCTEXT("InitialMapNotAvailable", "The Initial Map selected for launch is not in the build.").ToString();
		case ELauncherProfileValidationErrors::MalformedLaunchCommandLine:
			return LOCTEXT("MalformedLaunchCommandLine", "The specified launch command line is not formatted correctly.").ToString();
		case ELauncherProfileValidationErrors::NoBuildConfigurationSelected:
			return LOCTEXT("NoBuildConfigurationSelectedError", "A Build Configuration must be selected.").ToString();
		case ELauncherProfileValidationErrors::NoCookedCulturesSelected:
			return LOCTEXT("NoCookedCulturesSelectedError", "At least one Culture must be selected when cooking by the book.").ToString();
		case ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned:
			return LOCTEXT("NoLaunchRoleDeviceAssigned", "One or more launch roles do not have a device assigned.").ToString();
		case ELauncherProfileValidationErrors::NoPlatformSelected:
			return LOCTEXT("NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book.").ToString();
		case ELauncherProfileValidationErrors::NoProjectSelected:
			return LOCTEXT("NoBuildGameSelectedError", "A Project must be selected.").ToString();
		case ELauncherProfileValidationErrors::NoPackageDirectorySpecified:
			return LOCTEXT("NoPackageDirectorySpecified", "The deployment requires a package directory to be specified.").ToString();
		case ELauncherProfileValidationErrors::NoPlatformSDKInstalled:
			return LOCTEXT("NoPlatformSDKInstalled", "A required platform SDK is missing.").ToString();
		case ELauncherProfileValidationErrors::UnversionedAndIncrimental:
			return LOCTEXT("UnversionedAndIncrimental", "Unversioned build cannot be incremental.").ToString();
		case ELauncherProfileValidationErrors::GeneratingPatchesCanOnlyRunFromByTheBookCookMode:
			return LOCTEXT("GeneratingPatchesCanOnlyRunFromByTheBookCookMode", "Generating patch requires cook by the book mode.").ToString();
		case ELauncherProfileValidationErrors::GeneratingMultiLevelPatchesRequiresGeneratePatch:
			return LOCTEXT("GeneratingMultiLevelPatchesRequiresGeneratePatch", "Generating multilevel patch requires generating patch.").ToString();
		case ELauncherProfileValidationErrors::StagingBaseReleasePaksWithoutABaseReleaseVersion:
			return LOCTEXT("StagingBaseReleasePaksWithoutABaseReleaseVersion", "Staging base release pak files requires a base release version to be specified").ToString();
		case ELauncherProfileValidationErrors::GeneratingChunksRequiresCookByTheBook:
			return LOCTEXT("GeneratingChunksRequiresCookByTheBook", "Generating Chunks requires cook by the book mode.").ToString();
		case ELauncherProfileValidationErrors::GeneratingChunksRequiresUnrealPak:
			return LOCTEXT("GeneratingChunksRequiresUnrealPak", "UnrealPak must be selected to Generate Chunks.").ToString();
		case ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresGeneratingChunks:
			return LOCTEXT("GeneratingHttpChunkDataRequiresGeneratingChunks", "Generate Chunks must be selected to Generate Http Chunk Install Data.").ToString();
		case ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresValidDirectoryAndName:
			return LOCTEXT("GeneratingHttpChunkDataRequiresValidDirectoryAndName", "Generating Http Chunk Install Data requires a valid directory and release name.").ToString();
		case ELauncherProfileValidationErrors::ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly:
			return LOCTEXT("ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly", "Shipping doesn't support commandline options and can't use cook on the fly").ToString();
		case ELauncherProfileValidationErrors::CookOnTheFlyDoesntSupportServer:
			return LOCTEXT("CookOnTheFlyDoesntSupportServer", "Cook on the fly doesn't support server target configurations").ToString();
		case ELauncherProfileValidationErrors::LaunchDeviceIsUnauthorized:
			return LOCTEXT("LaunchDeviceIsUnauthorized", "Device is unauthorized or locked.").ToString();
		case ELauncherProfileValidationErrors::NoArchiveDirectorySpecified:
			return LOCTEXT("NoArchiveDirectorySpecifiedError", "The archive step requires a valid directory.").ToString();
		case ELauncherProfileValidationErrors::IoStoreRequiresPakFiles:
			return LOCTEXT("IoStoreRequiresPakFilesError", "UnrealPak must be selected when using I/O store container file(s)").ToString();
		case ELauncherProfileValidationErrors::BuildTargetCookVariantMismatch:
			return LOCTEXT("BuildTargetCookVariantMismatch", "Build Target and Cook Variant mismatch.").ToString();
		case ELauncherProfileValidationErrors::BuildTargetIsRequired:
			return LOCTEXT("BuildTargetIsRequired", "This profile requires an explicit Build Target set.").ToString();
		case ELauncherProfileValidationErrors::FallbackBuildTargetIsRequired:
			return LOCTEXT("FallbackBuildTargetIsRequired", "An explicit Default Build Target is required for the selected Variant.").ToString();
		case ELauncherProfileValidationErrors::CopyToDeviceRequiresNoPackaging:
			return LOCTEXT("CopyToDeviceRequiresNoPackaging", "Deployment by copying to device requires 'Do not package' packaging.").ToString();
		default:
			return TEXT("Unknown");
	};
}

#undef LOCTEXT_NAMESPACE

