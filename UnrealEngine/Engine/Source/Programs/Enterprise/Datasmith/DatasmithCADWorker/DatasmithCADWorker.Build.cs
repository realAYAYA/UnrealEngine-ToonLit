// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DatasmithCADWorker : ModuleRules
{
	public DatasmithCADWorker(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"ApplicationCore",
				"Sockets",
				"DatasmithDispatcher",
				"CADInterfaces",
				"CADTools",
			}
		);
	}
}
