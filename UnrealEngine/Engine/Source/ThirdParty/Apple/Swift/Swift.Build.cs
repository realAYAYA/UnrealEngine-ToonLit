// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Swift : ModuleRules
{
	public Swift(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		string PlatformPath = null;

        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PlatformPath = "iphoneos";
		}
		else if(Target.Platform == UnrealTargetPlatform.Mac)
		{
			PlatformPath = "macosx";
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PlatformPath = "appletvos";
		}
		else if (Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			PlatformPath = "xros";
		}

		if(PlatformPath != null)
		{
			PublicSystemLibraryPaths.AddRange(
				new string[] { 
					GetSwiftStandardLibraryLinkPath(PlatformPath), 
					"/usr/lib/swift"
				}
			);
		}
	}

	private static string GetSwiftStandardLibraryLinkPath(string PlatformPath)
	{
		string XcodeRoot = Utils.RunLocalProcessAndReturnStdOut("/usr/bin/xcode-select", "--print-path");
		return  $"{XcodeRoot}/Toolchains/XcodeDefault.xctoolchain/usr/lib/swift/{PlatformPath}";
	}
}

