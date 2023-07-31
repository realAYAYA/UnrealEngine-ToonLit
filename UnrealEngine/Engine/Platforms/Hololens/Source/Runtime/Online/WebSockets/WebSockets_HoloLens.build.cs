// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WebSockets_HoloLens : WebSockets
	{
		protected override bool bPlatformSupportsWinRTWebsockets
		{
			get => true;
		}

		public WebSockets_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}

		protected override string WebSocketsManagerPlatformInclude
		{
			get
			{
				return "HoloLensWebSocketsManager.h";
			}
		}

		protected override string WebSocketsManagerPlatformClass
		{
			get
			{
				return "FHoloLensWebSocketsManager";
			}
		}
	}
}
