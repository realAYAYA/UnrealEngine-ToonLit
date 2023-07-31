// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProxyLODMeshReduction : ModuleRules
	{
		public ProxyLODMeshReduction(ReadOnlyTargetRules Target) : base(Target)
		{
			// For boost:: and TBB:: code
			bUseRTTI = true;

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"TraceLog",
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"UnrealEd",
					"RawMesh",
					"MeshDescription",
					"StaticMeshDescription",
					"MeshUtilities",
					"MaterialUtilities",
					"PropertyEditor",
					"SlateCore",
					"Slate",
					"RenderCore",
					"RHI",
					"QuadricMeshReduction"
					// ... add other public dependencies that you statically link with here ...
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorFramework",
					"Engine",
					"UnrealEd"
					// ... add private dependencies that you statically link with here ...
				}
				);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"UVAtlas",
				"DirectXMesh",
				"OpenVDB"
			);
		}
	}
}
