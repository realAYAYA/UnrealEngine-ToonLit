// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EditorAnalyticsSession : ModuleRules
	{
		public EditorAnalyticsSession( ReadOnlyTargetRules Target ) : base( Target )
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Analytics",
					"AnalyticsET"
				}
			);
		}
	}
}
