// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OodleNetworkHandlerComponent_HoloLens : OodleNetworkHandlerComponent
	{
		public OodleNetworkHandlerComponent_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			throw new System.Exception("Should not be here - HoloLens in the uplugin's PlatformDenyList");
		}
	}
}
