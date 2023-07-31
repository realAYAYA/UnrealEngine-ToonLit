// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProceduralMeshComponent : ModuleRules
	{
		public ProceduralMeshComponent(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("../../../../Shaders/Shared");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MeshDescription",
					"RenderCore",
					"RHI",
					"StaticMeshDescription",
					"PhysicsCore"
				}
				);
		}
	}
}
