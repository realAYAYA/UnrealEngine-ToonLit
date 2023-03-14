// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
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
			MinVersion = "12.0.0";
			MaxVersion = null;
		}
	}
}