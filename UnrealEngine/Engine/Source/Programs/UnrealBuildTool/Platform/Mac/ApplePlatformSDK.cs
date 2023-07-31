// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using EpicGames.Core;
using System.Text.RegularExpressions;
using Microsoft.Win32;
using System.Diagnostics;
using Microsoft.Extensions.Logging;

///////////////////////////////////////////////////////////////////
// If you are looking for supported version numbers, look in the
// ApplePlatformSDK.Versions.cs file next to this file, and
// als IOS/IOSPlatformSDK.Versions.cs
///////////////////////////////////////////////////////////////////

namespace UnrealBuildTool
{
	internal partial class ApplePlatformSDK : UEBuildPlatformSDK
	{
		public ApplePlatformSDK(ILogger Logger)
			: base(Logger)
		{
		}

		public override bool TryConvertVersionToInt(string? StringValue, out UInt64 OutValue, string? Hint)
		{
			return UnrealBuildBase.ApplePlatformSDK.TryConvertVersionToInt(StringValue, out OutValue);
		}

		protected override string? GetInstalledSDKVersion()
		{
			return UnrealBuildBase.ApplePlatformSDK.InstalledSDKVersion;
		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			SDKStatus Status = base.HasRequiredManualSDKInternal();

			// iTunes is technically only need to deploy to and run on connected devices.
			// This code removes requirement for Windows builders to have Xcode installed.
			if (Status == SDKStatus.Invalid && !RuntimePlatform.IsMac && Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
            {
				Status = SDKStatus.Valid;
            }
			return Status;
		}
	}
}