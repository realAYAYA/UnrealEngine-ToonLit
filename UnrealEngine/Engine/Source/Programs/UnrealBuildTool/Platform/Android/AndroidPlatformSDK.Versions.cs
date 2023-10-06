// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class AndroidPlatformSDK : UEBuildPlatformSDK
	{
		protected override string GetMainVersionInternal()
		{
			return "r25b";
		}

		public override string GetAutoSDKDirectoryForMainVersion()
		{
			return "-26";
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
				case "platforms": return "android-33";
				case "build-tools": return "33.0.1";
				case "cmake": return "3.10.2.4988404";
				case "ndk": return "25.1.8937393";
			}

			return "";
		}
	}
}
