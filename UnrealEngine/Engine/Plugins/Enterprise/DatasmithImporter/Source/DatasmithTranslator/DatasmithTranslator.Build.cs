// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithTranslator : ModuleRules
	{
		public DatasmithTranslator(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithCore",
					"DatasmithContent",
					"XmlParser",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"MeshDescription",
					"RawMesh",
					"StaticMeshDescription",
				}
			);
		}
	}
}
