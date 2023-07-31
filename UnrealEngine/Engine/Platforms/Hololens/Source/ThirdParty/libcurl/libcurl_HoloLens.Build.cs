// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class libcurl_HoloLens : libcurl
	{
		public libcurl_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			// We do not currently have hololens OpenSSL binaries, lets not pretend we do.
			// We will remove WITH_LIBCURL=1 which should mean we *should* compile out dependencies on it, but I'm not very confident that 
			// there won't be some runtime failures in projects that do depend on it (like EngineTest).

			PublicDefinitions.Remove("WITH_LIBCURL=1");

		}
	}
}
