// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2019Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2019Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2019";
		ExeBinariesSubFolder = @"3DSMax\2019";

		AddCopyPostBuildStep(Target);
	}
}
