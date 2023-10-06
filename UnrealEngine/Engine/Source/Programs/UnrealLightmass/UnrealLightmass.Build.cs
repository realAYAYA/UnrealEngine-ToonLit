// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealLightmass : ModuleRules
{
	public UnrealLightmass(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "zlib", "SwarmInterface", "Projects", "ApplicationCore" });

		PublicDefinitions.Add("UE_LIGHTMASS=1");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Unreallightmass requires GetProcessMemoryInfo exported by psapi.dll. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683219(v=vs.85).aspx
			PublicSystemLibraries.Add("psapi.lib");
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Messaging",
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			// On Mac/Linux UnrealLightmass is executed locally and communicates with the editor using Messaging module instead of SwarmAgent
			// @todo: allow for better plug-in support in standalone Slate apps
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Networking",
					"Sockets",
					"Messaging",
					"UdpMessaging",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
				}
			);
		}
		
		// Lightmass ray tracing is 8% faster with buffer security checks disabled due to fixed size arrays on the stack in the kDOP ray tracing functions
		// Warning: This means buffer overwrites will not be detected
		bEnableBufferSecurityChecks = false;

		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/Launch");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/ImportExport");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/CPUSolver");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/Lighting");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Misc");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Math");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Templates");

        // Always use the official version of IntelTBB
        string IntelTBBLibs = Target.UEThirdPartyBinariesDirectory + "Intel/TBB/";
        string IntelEmbreeLibs = Target.UEThirdPartyBinariesDirectory + "Intel/Embree/Embree270";

        // EMBREE
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree270/Win64/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicAdditionalLibraries.Add(SDKDir + "lib/embree.lib");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/embree.dll");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/tbb.dll", IntelTBBLibs + "Win64/tbb.dll");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/tbbmalloc.dll", IntelTBBLibs + "Win64/tbbmalloc.dll");
            PublicDefinitions.Add("USE_EMBREE=1");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            string SDKDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree270/MacOSX/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicAdditionalLibraries.Add(SDKDir + "lib/libembree.2.dylib");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/Mac/libembree.2.dylib");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/Mac/libtbb.dylib", IntelTBBLibs + "Mac/libtbb.dylib"); // Take latest version to avoid overwriting the editor's copy
			RuntimeDependencies.Add("$(EngineDir)/Binaries/Mac/libtbbmalloc.dylib", IntelTBBLibs + "Mac/libtbbmalloc.dylib");
			PublicDefinitions.Add("USE_EMBREE=1");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64) // no support for arm64 yet
		{
			string IncludeDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree270/Linux/x86_64-unknown-linux-gnu";
			string SDKDir = Path.Combine(IntelEmbreeLibs, "Linux/x86_64-unknown-linux-gnu/lib");

			PublicIncludePaths.Add(Path.Combine(IncludeDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(SDKDir, "libembree.so"));
			PublicAdditionalLibraries.Add(Path.Combine(IntelTBBLibs, "Linux/libtbb.so"));
			// disabled for Linux atm due to a bug in libtbbmalloc on exit
			// PublicAdditionalLibraries.Add(Path.Combine(IntelTBBLibs, "Linux/libtbbmalloc.so"));
			RuntimeDependencies.Add(Path.Combine(SDKDir, "libembree.so"));
			RuntimeDependencies.Add(Path.Combine(SDKDir, "libembree.so.2"));
			RuntimeDependencies.Add(Path.Combine(IntelTBBLibs, "Linux/libtbb.so"));
			RuntimeDependencies.Add(Path.Combine(IntelTBBLibs, "Linux/libtbb.so.2"));
			// disabled for Linux atm due to a bug in libtbbmalloc on exit
			// RuntimeDependencies.Add(Path.Combine(IntelTBBLibs, "Linux/libtbbmalloc.so"));
			PublicDefinitions.Add("USE_EMBREE=1");
		}
        else
        {
            PublicDefinitions.Add("USE_EMBREE=0");
        }
    }
}
