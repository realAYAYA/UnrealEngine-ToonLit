// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace AutomationTool
{
	[Help("Checks that the installed Xcode version is the version specified.")]
	[Help("-Version", "The expected version number")]
	class CheckXcodeVersion : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string RequestedVersion = ParseParamValue("Version");
			if(RequestedVersion == null)
			{
				throw new AutomationException("Missing -Version=... parameter");
			}

			string InstalledSdkVersion = UnrealBuildBase.ApplePlatformSDK.InstalledSDKVersion;

			if (InstalledSdkVersion == null)
			{
				throw new AutomationException("Unable to query version number from xcodebuild");
			}
			
			if (InstalledSdkVersion != RequestedVersion)
			{
				Logger.LogWarning("Installed Xcode version is {InstalledSdkVersion} - expected {RequestedVersion}", InstalledSdkVersion, RequestedVersion);
			}
		}
	}
}
