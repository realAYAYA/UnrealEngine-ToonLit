// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class XInputDevice : ModuleRules
	{
		public XInputDevice(ReadOnlyTargetRules Target) : base(Target)
		{
			// Enable cast warnings as errors
			UnsafeTypeCastWarningLevel = WarningLevel.Error;
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"ApplicationCore",
					"Engine",
					"InputDevice",
				}
			);

			// XInput is only supported on Windows
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"XInput"
				);
			}
			else
			{
				Console.Error.Write("XInput is only supported on the Windows target platform group!");
			}
		}
	}
}