// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	/// <summary>
	/// Module designed to serve as the home for all validation systems running in the engine. 
	/// </summary>
	public class MutableValidation : ModuleRules
	{
		public MutableValidation(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "MuV";

			DefaultBuildSettings = BuildSettingsVersion.V2;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Settings", 
					"Engine", 
					"MutableRuntime"
				});
			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					
					"DataValidation",
					"CustomizableObject", 
					"CustomizableObjectEditor", 
					"MutableTools",
				}
			);
		}
	}
}