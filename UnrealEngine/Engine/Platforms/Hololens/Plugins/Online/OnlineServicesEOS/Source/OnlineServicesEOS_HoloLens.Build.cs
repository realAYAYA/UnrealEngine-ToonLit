// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OnlineServicesEOS_HoloLens : OnlineServicesEOS
	{
		public OnlineServicesEOS_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			throw new System.Exception("Should not be here - HoloLens in the uplugin's PlatformDenyList");
		}
	}
}
