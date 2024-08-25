// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

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
			if (Status == SDKStatus.Invalid && !RuntimePlatform.IsMac && Unreal.IsBuildMachine())
			{
				Status = SDKStatus.Valid;
			}
			return Status;
		}
	}
}