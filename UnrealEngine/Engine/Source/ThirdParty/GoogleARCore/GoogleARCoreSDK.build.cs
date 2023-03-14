// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GoogleARCoreSDK : ModuleRules
{
	public GoogleARCoreSDK(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

		string ARCoreSDKDir = Target.UEThirdPartySourceDirectory + "GoogleARCore/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					ARCoreSDKDir + "include/",
				}
			);

		string ARCoreSDKBaseLibPath = ARCoreSDKDir + "lib/";
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(ARCoreSDKBaseLibPath + "arm64-v8a/libarcore_sdk_c.so");
		}
		else if(Target.Platform == UnrealTargetPlatform.IOS)
		{
			string ARCoreSDKiOSLibPath = ARCoreSDKBaseLibPath + "ios/";
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGTMSessionFetcher.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGoogleToolboxForMac.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libProtobuf.a");

			PublicSystemLibraries.Add("c++");
			PublicSystemLibraries.Add("sqlite3");
			PublicSystemLibraries.Add("z");

			PublicFrameworks.Add("ARKit");
			PublicFrameworks.Add("AVFoundation");
			PublicFrameworks.Add("CoreGraphics");
			PublicFrameworks.Add("CoreImage");
			PublicFrameworks.Add("CoreMotion");
			PublicFrameworks.Add("CoreMedia");
			PublicFrameworks.Add("CoreVideo");
			PublicFrameworks.Add("Foundation");
			PublicFrameworks.Add("ImageIO");
			PublicFrameworks.Add("QuartzCore");
			PublicFrameworks.Add("Security");
			PublicFrameworks.Add("UIKit");
			PublicFrameworks.Add("VideoToolbox");

			PublicAdditionalFrameworks.Add(new Framework("ARCoreCloudAnchors", "lib/ios/ARCoreCloudAnchors.embeddedframework.zip", "ARCoreCloudAnchors.framework/Resources/ARCoreResources.bundle"));
		}
	}
}
