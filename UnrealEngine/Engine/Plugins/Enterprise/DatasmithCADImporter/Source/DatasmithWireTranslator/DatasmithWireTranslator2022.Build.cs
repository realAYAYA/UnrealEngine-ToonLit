// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class DatasmithWireTranslator2022 : DatasmithWireTranslatorBase
{
	public DatasmithWireTranslator2022(ReadOnlyTargetRules Target) 
		: base(Target)
	{
	}

	public override string GetAliasVersion()
	{
		return "OpenModel2022";
	}
	
	public override string GetAliasDefinition()
	{
	return "OPEN_MODEL_2022";
	}
}