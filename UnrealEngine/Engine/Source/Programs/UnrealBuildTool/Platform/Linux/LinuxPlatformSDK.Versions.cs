// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class LinuxPlatformSDK : UEBuildPlatformSDK
	{
		public override string GetMainVersion()
		{
			return "v20_clang-13.0.1-centos7";
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			// all that matters is the number after the v, according to TryConvertVersionToInt()
			MinVersion = "v20_clang-13.0.1-centos7";
			MaxVersion = "v20_clang-13.0.1-centos7";
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}
	}
}
