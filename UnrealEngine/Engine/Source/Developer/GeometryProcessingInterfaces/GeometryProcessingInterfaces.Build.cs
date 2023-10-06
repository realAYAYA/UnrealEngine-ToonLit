// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GeometryProcessingInterfaces : ModuleRules
{
	public GeometryProcessingInterfaces(ReadOnlyTargetRules Target) : base(Target)
	{		
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("GeometryCore");
	}
}
