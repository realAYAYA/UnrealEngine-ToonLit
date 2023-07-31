// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class FBX : ModuleRules
{
	public FBX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string FBXSDKDir = Target.UEThirdPartySourceDirectory + "FBX/2020.2/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					FBXSDKDir + "include",
					FBXSDKDir + "include/fbxsdk",
				}
			);

		string FBXDLLDir = Target.UEThirdPartyBinariesDirectory + "FBX/2020.2/";

		if ( Target.Platform == UnrealTargetPlatform.Win64 )
		{
			string FBxLibPath = FBXSDKDir + "lib/vs2017/";
			string FBxDllPath = FBXDLLDir + "Win64/libfbxsdk.dll";

			FBxLibPath += "x64/release/";

			if (Target.LinkType != TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(FBxLibPath + "libfbxsdk.lib");

				// We are using DLL versions of the FBX libraries
				PublicDefinitions.Add("FBXSDK_SHARED");

				RuntimeDependencies.Add("$(TargetOutputDir)/libfbxsdk.dll", FBxDllPath);
			}
			else
			{
				if (Target.bUseStaticCRT)
				{
					PublicAdditionalLibraries.Add(FBxLibPath + "libfbxsdk-mt.lib");
					PublicAdditionalLibraries.Add(FBxLibPath + "libxml2-mt.lib");
					PublicAdditionalLibraries.Add(FBxLibPath + "zlib-mt.lib");
				}
				else
				{
					PublicAdditionalLibraries.Add(FBxLibPath + "libfbxsdk-md.lib");
					PublicAdditionalLibraries.Add(FBxLibPath + "libxml2-md.lib");
					PublicAdditionalLibraries.Add(FBxLibPath + "zlib-md.lib");
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string FBxDllPath = FBXDLLDir + "Mac/libfbxsdk.dylib";
			PublicAdditionalLibraries.Add(FBxDllPath);
			RuntimeDependencies.Add(FBxDllPath);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string FBxDllPath = FBXDLLDir + "Linux/" + Target.Architecture + "/";
			if (!Target.bIsEngineInstalled && !Directory.Exists(FBxDllPath))
			{
				string Err = string.Format("FBX SDK not found in {0}", FBxDllPath);
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			PublicDefinitions.Add("FBXSDK_SHARED");

			PublicRuntimeLibraryPaths.Add(FBxDllPath);
			PublicAdditionalLibraries.Add(FBxDllPath + "libfbxsdk.so");
			RuntimeDependencies.Add(FBxDllPath + "libfbxsdk.so");

			/* There is a bug in fbxarch.h where is doesn't do the check
			 * for clang under linux */
			PublicDefinitions.Add("FBXSDK_COMPILER_CLANG");

			// libfbxsdk has been built against libstdc++ and as such needs this library
			PublicSystemLibraries.Add("stdc++");

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"libxml2",
				"zlib"
			});
		}
	}
}
