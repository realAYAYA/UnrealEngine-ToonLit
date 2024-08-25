// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	class TestUnrealInstallAndRunMac : TestUnrealInstallAndRunDesktop
	{
		public TestUnrealInstallAndRunMac()
		{
			Platform = UnrealTargetPlatform.Mac;
		}
	}
}