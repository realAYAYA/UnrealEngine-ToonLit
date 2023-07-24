// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"

namespace BuildPatchServices
{
	/**
	 * An enum defining the verification mode that should be used.
	 */
	enum class EVerifyMode : uint32
	{
		// Fully SHA checks all files in the build.
		ShaVerifyAllFiles,

		// Fully SHA checks only files touched by the install/patch process.
		ShaVerifyTouchedFiles,

		// Checks just the existence and file size of all files in the build.
		FileSizeCheckAllFiles,

		// Checks just the existence and file size of only files touched by the install/patch process.
		FileSizeCheckTouchedFiles
	};

	/**
	 * An enum defining the possible causes for a verification failure.
	 */
	enum class EVerifyError : uint32
	{
		// The file was not found.
		FileMissing,

		// The file failed to open.
		OpenFileFailed,

		// The file failed its hash check.
		HashCheckFailed,

		// The file was not the expected size.
		FileSizeFailed
	};

	/**
	 * Returns the string representation of the EVerifyMode value. Used for analytics and logging only.
	 * @param  VerifyMode     The value.
	 * @return the enum's string representation.
	 */
	inline const FString& EnumToString(const EVerifyMode& VerifyMode)
	{
		// Const enum strings, special case no error.
		static const FString ShaVerifyAllFiles(TEXT("EVerifyMode::ShaVerifyAllFiles"));
		static const FString ShaVerifyTouchedFiles(TEXT("EVerifyMode::ShaVerifyTouchedFiles"));
		static const FString FileSizeCheckAllFiles(TEXT("EVerifyMode::FileSizeCheckAllFiles"));
		static const FString FileSizeCheckTouchedFiles(TEXT("EVerifyMode::FileSizeCheckTouchedFiles"));
		static const FString InvalidOrMax(TEXT("InvalidOrMax"));

		switch (VerifyMode)
		{
			case EVerifyMode::ShaVerifyAllFiles: return ShaVerifyAllFiles;
			case EVerifyMode::ShaVerifyTouchedFiles: return ShaVerifyTouchedFiles;
			case EVerifyMode::FileSizeCheckAllFiles: return FileSizeCheckAllFiles;
			case EVerifyMode::FileSizeCheckTouchedFiles: return FileSizeCheckTouchedFiles;
			default: return InvalidOrMax;
		}
	}
}

ENUM_RANGE_BY_FIRST_AND_LAST(BuildPatchServices::EVerifyError, BuildPatchServices::EVerifyError::FileMissing, BuildPatchServices::EVerifyError::FileSizeFailed)