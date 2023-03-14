// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class AndroidPlatformSDK : UEBuildPlatformSDK
	{
		public override string GetMainVersion()
		{
			return "r25b";
		}
		
		public override string GetAutoSDKDirectoryForMainVersion()
		{
			return "-25";
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = "r25b";
			MaxVersion = "r25b";
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}

		public override string GetPlatformSpecificVersion(string VersionType)
		{
			switch (VersionType.ToLower())
			{
				case "platforms": return "android-32";
				case "build-tools": return "30.0.3";
				case "cmake": return "3.10.2.4988404";
				case "ndk": return "25.1.8937393";
			}

			return "";
		}
	}
}
