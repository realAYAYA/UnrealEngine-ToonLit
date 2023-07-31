// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class Nanosvg : ModuleRules
{
	public Nanosvg(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("NSVG_USE_BGRA=1");

		Type = ModuleType.External;
	}
}
