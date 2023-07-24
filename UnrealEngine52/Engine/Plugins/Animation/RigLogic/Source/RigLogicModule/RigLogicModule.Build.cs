// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RigLogicModule : ModuleRules
	{
		public RigLogicModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ControlRig",
					"RigLogicLib",
					"RigVM",
					"Projects"
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PublicDependencyModuleNames.Add("UnrealEd");
				PublicDependencyModuleNames.Add("EditorFramework");
				PublicDependencyModuleNames.Add("MessageLog");

				PrivateDependencyModuleNames.Add("SkeletalMeshUtilitiesCommon");
				PrivateDependencyModuleNames.Add("RHI");
				PrivateDependencyModuleNames.Add("RenderCore");
			}

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationCore"
				}
			);
		}
	}
}
