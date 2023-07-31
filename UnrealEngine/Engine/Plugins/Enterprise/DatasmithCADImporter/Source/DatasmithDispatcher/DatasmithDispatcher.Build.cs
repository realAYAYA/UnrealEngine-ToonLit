// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithDispatcher : ModuleRules
	{
		public DatasmithDispatcher(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
					"Sockets",
					"CADInterfaces",
					"CADTools",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                }
			);
        }
    }
}
