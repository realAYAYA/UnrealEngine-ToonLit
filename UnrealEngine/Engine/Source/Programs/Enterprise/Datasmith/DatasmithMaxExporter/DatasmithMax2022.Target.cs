// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2022Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2022Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2022";
		ExeBinariesSubFolder = @"3DSMax\2022";



		AddCopyPostBuildStep(Target);
	}
}
