// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class LinuxPlatformSDK : UEBuildPlatformSDK
	{
		protected override string GetMainVersionInternal()
		{
			return "v22_clang-16.0.6-centos7";
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			// all that matters is the number after the v, according to TryConvertVersionToInt()
			MinVersion = "v22_clang-16.0.6-centos7";
			MaxVersion = "v22_clang-16.0.6-centos7";
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}
	}
}
