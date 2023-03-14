// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MRMesh : ModuleRules
	{
		public MRMesh(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
					"PhysicsCore"
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
			
			// Used for including the private Chaos headers
			string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
			PrivateIncludePaths.Add(Path.Combine(EnginePath, "Source/Runtime/Engine/Private"));

			PrivateIncludePathModuleNames.Add("DerivedDataCache");
		}
	}
}
