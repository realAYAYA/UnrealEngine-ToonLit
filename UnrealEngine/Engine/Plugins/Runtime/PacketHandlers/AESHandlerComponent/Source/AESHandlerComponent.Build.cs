// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AESHandlerComponent : ModuleRules
{
	protected virtual bool DefaultToSSL { get { return true; } }

	public AESHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PacketHandler",
				"PlatformCrypto",
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"PlatformCrypto"
			}
			);

		if (DefaultToSSL)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"PlatformCryptoOpenSSL",
				}
				);
		}
	}
}