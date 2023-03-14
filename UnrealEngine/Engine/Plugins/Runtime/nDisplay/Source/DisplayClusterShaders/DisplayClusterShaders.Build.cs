// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterShaders : ModuleRules
{
	public DisplayClusterShaders(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		// This define required for mpcdi library
		PublicDefinitions.Add("MPCDI_STATIC");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Projects"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DisplayCluster",
				"RenderCore",
				"RHI",
				"Renderer",
				"ProceduralMeshComponent",
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
		
		// NOTE: General rule is not to access the private folder of another module
		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
		});

		AddEngineThirdPartyPrivateStaticDependencies(Target, "UElibPNG");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

		AddThirdPartyDependencies(ROTargetRules);
	}

	public void AddThirdPartyDependencies(ReadOnlyTargetRules ROTargetRules)
	{
		string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/"));

		string PathLib = string.Empty;
		string PathInc = string.Empty;

		// MPCDI
		PathLib = Path.Combine(ThirdPartyPath, "MPCDI/Lib");
		PathInc = Path.Combine(ThirdPartyPath, "MPCDI/Include");

		// Libs
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(PathLib, "mpcdi.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(PathLib, "tinyxml2.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
		{
			PublicAdditionalLibraries.Add(Path.Combine(PathLib, "libmpcdi.a"));
		}

		// Include paths
		PrivateIncludePaths.Add(PathInc);
		PrivateIncludePaths.Add(Path.Combine(PathInc, "Base"));
		PrivateIncludePaths.Add(Path.Combine(PathInc, "Container"));
		PrivateIncludePaths.Add(Path.Combine(PathInc, "Creators"));
		PrivateIncludePaths.Add(Path.Combine(PathInc, "IO"));
		PrivateIncludePaths.Add(Path.Combine(PathInc, "Utils"));
	}
}
