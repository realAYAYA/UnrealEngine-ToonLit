// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.XLocLocalization;

public class XLocLocalizationProvider_Sample : XLocLocalizationProvider
{
	public XLocLocalizationProvider_Sample(LocalizationProviderArgs InArgs)
		: base(InArgs)
	{
		Config.Server = Command.ParseParamValue("Server");
		Config.APIKey = Command.ParseParamValue("APIKey");
		Config.APISecret = Command.ParseParamValue("APISecret");
		Config.LocalizationId = Command.ParseParamValue("LocalizationId");
	}

	public static string StaticGetLocalizationProviderId()
	{
		return "XLoc_Sample";
	}

	public override string GetLocalizationProviderId()
	{
		return StaticGetLocalizationProviderId();
	}
}
