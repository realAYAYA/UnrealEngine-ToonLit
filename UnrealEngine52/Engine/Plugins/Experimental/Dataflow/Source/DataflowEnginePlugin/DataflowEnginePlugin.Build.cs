// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowEnginePlugin : ModuleRules
	{
		public DataflowEnginePlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			bTreatAsEngineModule = true;
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"GeometryFramework"
				}
			);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// NOTE: UVEditorTools is a separate module so that it doesn't rely on the editor.
					// So, do not add editor dependencies here.
	
					"Engine",
					"RenderCore",
					"RHI",
					"DataflowCore",
					"DataflowEngine",
					"Chaos",
					"GeometryCore",
					"DynamicMesh"
				}
			);
		}
	}
}
