// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.CrowdinLocalization;

public class CrowdinLocalizationProvider_Sample : CrowdinLocalizationProvider
{
	public CrowdinLocalizationProvider_Sample(LocalizationProviderArgs InArgs)
		: base(InArgs)
	{
		Config.ProjectId = Command.ParseParamValue("ProjectId");
		Config.AccessToken = Command.ParseParamValue("AccessToken");
	}

	public static string StaticGetLocalizationProviderId()
	{
		return "Crowdin_Sample";
	}

	public override string GetLocalizationProviderId()
	{
		return StaticGetLocalizationProviderId();
	}
}
