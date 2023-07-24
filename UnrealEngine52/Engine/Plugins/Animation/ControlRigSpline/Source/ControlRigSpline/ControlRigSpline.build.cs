// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ControlRigSpline : ModuleRules
	{
		public ControlRigSpline(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RigVM",
				"ControlRig",
			});

			// TODO: Should not be including private headers in public code
			PublicIncludePaths.Add(Path.Combine(GetModuleDirectory("ControlRig"), "Private/Units/Highlevel/Hierarchy")); // For RigUnit_FitChainToCurve.h
		}
	}
}
