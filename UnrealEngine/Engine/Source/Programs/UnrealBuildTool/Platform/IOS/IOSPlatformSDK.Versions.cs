// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	internal class IOSPlatformSDK : ApplePlatformSDK
	{
		public IOSPlatformSDK(ILogger Logger)
			: base(Logger)
		{
		}

		protected override void GetValidSoftwareVersionRange(out string MinVersion, out string? MaxVersion)
		{
			// what is our min IOS version?
			MinVersion = "15.0";
			MaxVersion = null;
		}

		/// <summary>
		/// The minimum Clang version that a static library can be built with
		/// </summary>
		public Version MinimumStaticLibClangVersion
		{
			get
			{
				// return new Version("1200.00.32.29");	// UE 5.0 (Xcode 12.4)
				// return new Version("1300.00.29.30"); // (old) UE 5.1 sets Xcode13.2.1 as minimum supported
				return new Version("1316.0.21.2"); // UE 5.1 sets Xcode13.4 as minimum supported
			}
		}
	}
}
