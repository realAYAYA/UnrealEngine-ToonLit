// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;

namespace UnrealBuildTool
{
	internal partial class ApplePlatformSDK : UEBuildPlatformSDK
	{
		protected override string GetMainVersionInternal()
		{
			// Xcode prefered version?
			return "14.1";
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			if (RuntimePlatform.IsMac)
			{
				MinVersion = "14.1.0";
				MaxVersion = "15.9.9";
			}
			else
			{
				// @todo turnkey: these are MobileDevice .dll versions in Windows - to get the iTunes app version (12.3.4.1 etc) would need to hunt down the .exe
				MinVersion = "1100.0.0.0";
				MaxVersion = "8999.0";
			}
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}

		/// <summary>
		/// The minimum macOS SDK version that a dynamic library can be built with
		/// </summary>
		public virtual Version MinimumDynamicLibSDKVersion => new Version("12.1");      // SDK used in Xcode13.2.1

	}
}
