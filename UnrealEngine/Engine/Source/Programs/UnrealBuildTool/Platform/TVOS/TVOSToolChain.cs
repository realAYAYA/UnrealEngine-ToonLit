// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	class TVOSToolChainSettings : IOSToolChainSettings
	{
		public TVOSToolChainSettings(ILogger Logger) : base("AppleTVOS", "AppleTVSimulator", "tvos", Logger)
		{
		}
	}

	class TVOSToolChain : IOSToolChain
	{
		public TVOSToolChain(ReadOnlyTargetRules InTarget, TVOSProjectSettings InProjectSettings, ILogger InLogger)
			: base(InTarget, InProjectSettings, () => new TVOSToolChainSettings(InLogger), ClangToolChainOptions.None, InLogger)
		{
		}

		public override string GetXcodeMinVersionParam(UnrealArch Architecture)
	{
			return "tvos-version-min";
		}
	}
}
