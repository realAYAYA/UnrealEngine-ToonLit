// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	internal partial class MacPlatformSDK : ApplePlatformSDK
	{
		public MacPlatformSDK(ILogger Logger)
			: base(Logger)
		{
		}

		protected override void GetValidSoftwareVersionRange(out string MinVersion, out string? MaxVersion)
		{
			MinVersion = "12.5.0";      // macOS Monterey 12.5 (Released 2022/07/22)
			MaxVersion = null;
		}
	}
}