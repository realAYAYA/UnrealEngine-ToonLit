// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public abstract class DatasmithWireTranslatorBase : ModuleRules
{
	public DatasmithWireTranslatorBase(ReadOnlyTargetRules Target) : base(Target)
	{
		// TODO: investigate to remove that (Jira UETOOL-4975)
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CADInterfaces",
				"CADKernel",
				"CADKernelSurface",
				"CADLibrary",
				"CADTools",
				"DatasmithContent",
				"DatasmithCore",
				"DatasmithTranslator",
				"Engine",
				"MeshDescription",
				"ParametricSurface",
				"StaticMeshDescription",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
 					"MessageLog",
 					"UnrealEd",
				}
			);
		}

		PublicDefinitions.Add(GetAliasDefinition());
		PublicDefinitions.Add($"UE_DATASMITHWIRETRANSLATOR_NAMESPACE={GetAliasDefinition()}Namespace");
		PublicDefinitions.Add($"UE_DATASMITHWIRETRANSLATOR_MODULE_NAME={GetType().Name}");

		if (System.Type.GetType(GetAliasVersion()) != null)
		{
			PrivateDependencyModuleNames.Add(GetAliasVersion());
		}
	}

	public abstract string GetAliasVersion();
	public abstract string GetAliasDefinition();

}

public class DatasmithWireTranslator2020 : DatasmithWireTranslatorBase
{
	public DatasmithWireTranslator2020(ReadOnlyTargetRules Target) 
		: base(Target)
	{
	}

	public override string GetAliasVersion()
	{
		return "OpenModel2020";
	}
	
	public override string GetAliasDefinition()
	{
		return "OPEN_MODEL_2020";
	}
}
