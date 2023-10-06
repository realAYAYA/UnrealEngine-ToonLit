// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfigurationLog.h"
#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterConfigurationTypes.h"


namespace JSON426
{
	/**
	 * Auxiliary class with different type conversion functions
	 */
	class DisplayClusterConfigurationJsonHelpers
	{
	public:
		//////////////////////////////////////////////////////////////////////////////////////////////
		// TYPE --> STRING
		//////////////////////////////////////////////////////////////////////////////////////////////
		template <typename ConvertFrom>
		static FString ToString(const ConvertFrom& From);

		//////////////////////////////////////////////////////////////////////////////////////////////
		// STRING --> TYPE
		//////////////////////////////////////////////////////////////////////////////////////////////
		template <typename ConvertTo>
		static ConvertTo FromString(const FString& From);
	};


	// EDisplayClusterConfigurationEyeStereoOffset
	template <>
	inline FString DisplayClusterConfigurationJsonHelpers::ToString<>(const EDisplayClusterConfigurationEyeStereoOffset& From)
	{
		switch (From)
		{
		case EDisplayClusterConfigurationEyeStereoOffset::None:
			return DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetNone;

		case EDisplayClusterConfigurationEyeStereoOffset::Left:
			return DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetLeft;

		case EDisplayClusterConfigurationEyeStereoOffset::Right:
			return DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetRight;

		default:
			UE_LOG(LogDisplayClusterConfiguration, Error, TEXT("Unexpected camera stereo offset type"));
			break;
		}

		return DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetNone;
	}

	template <>
	inline EDisplayClusterConfigurationEyeStereoOffset DisplayClusterConfigurationJsonHelpers::FromString<>(const FString& From)
	{
		EDisplayClusterConfigurationEyeStereoOffset Result = EDisplayClusterConfigurationEyeStereoOffset::None;

		if (From.Equals(DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetNone, ESearchCase::IgnoreCase))
		{
			Result = EDisplayClusterConfigurationEyeStereoOffset::None;
		}
		else if (From.Equals(DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetLeft, ESearchCase::IgnoreCase))
		{
			Result = EDisplayClusterConfigurationEyeStereoOffset::Left;
		}
		else if (From.Equals(DisplayClusterConfigurationStrings::config::scene::camera::CameraStereoOffsetRight, ESearchCase::IgnoreCase))
		{
			Result = EDisplayClusterConfigurationEyeStereoOffset::Right;
		}

		return Result;
	}
}
