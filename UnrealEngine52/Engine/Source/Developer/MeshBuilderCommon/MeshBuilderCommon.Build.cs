// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    //MeshBuilder module is a editor module
	public class MeshBuilderCommon : ModuleRules
	{
		public MeshBuilderCommon(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "Engine",
                }
			);

            AddEngineThirdPartyPrivateStaticDependencies(Target,
                "nvTriStrip",
                "ForsythTriOptimizer",
                "nvTessLib"
                );
       }
	}
}
