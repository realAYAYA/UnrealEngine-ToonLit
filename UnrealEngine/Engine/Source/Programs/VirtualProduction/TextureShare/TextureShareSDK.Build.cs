// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class TextureShareSDK : ModuleRules
	{
		public TextureShareSDK(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bRequiresImplementModule = false;
			PrivatePCHHeaderFile = "Public/ITextureShareSDK.h";

			PrivateIncludePaths.AddRange(
				new string[] {
					Path.Combine(EngineDirectory,"Plugins/VirtualProduction/TextureShare/Source/TextureShareCore/Private"),
				}
			);

			PrivateDependencyModuleNames.AddRange( 
				new string[]
				{
					"Core",
					"TextureShareCore"
				}
			);
		}
	}
}
