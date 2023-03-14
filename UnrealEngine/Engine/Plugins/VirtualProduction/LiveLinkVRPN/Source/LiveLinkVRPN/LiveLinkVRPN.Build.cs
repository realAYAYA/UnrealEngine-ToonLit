// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkVRPN : ModuleRules
{
	public LiveLinkVRPN(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Networking",
				"Sockets",
				"LiveLinkInterface",
				"Messaging",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			}
			);

		// 3rd party dependencies
		AddThirdPartyDependencies(Target);
	}

	public void AddThirdPartyDependencies(ReadOnlyTargetRules Target)
	{
		string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/"));

		// VRPN
		string PathLib = Path.Combine(ThirdPartyPath, "VRPN/Lib");
		string PathInc = Path.Combine(ThirdPartyPath, "VRPN/Include");
		PublicAdditionalLibraries.Add(Path.Combine(PathLib, "vrpn.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(PathLib, "quat.lib"));
		PublicIncludePaths.Add(PathInc);
	}
}
