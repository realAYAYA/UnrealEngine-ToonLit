// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class CADKernel : ModuleRules
	{
		public CADKernel(ReadOnlyTargetRules Target)
			: base(Target)
		{
			DeterministicWarningLevel = WarningLevel.Off; // __DATE__ in Private/CADKernel/Core/System.cpp

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);
		}
	}
}