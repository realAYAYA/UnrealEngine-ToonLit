// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Validates the various platforms to determine if they are ready for building
	/// </summary>
	[ToolMode("ValidatePlatforms", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatformsForValidation | ToolModeOptions.SingleInstance)]
	class ValidatePlatformsMode : ToolMode
	{
		/// <summary>
		/// Platforms to validate
		/// </summary>
		[CommandLine("-Platforms=", ListSeparator = '+')]
		HashSet<UnrealTargetPlatform> Platforms = new HashSet<UnrealTargetPlatform>();

		/// <summary>
		/// Whether to validate all platforms
		/// </summary>
		[CommandLine("-AllPlatforms")]
		bool bAllPlatforms = false;

		/// <summary>
		/// Whether to output SDK versions.
		/// </summary>
		[CommandLine("-OutputSDKs")]
		bool bOutputSDKs = false;

		/// <summary>
		/// Executes the tool with the given arguments
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		/// <param name="Logger"></param>
		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			// Output a message if there are any arguments that are still unused
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// If the -AllPlatforms argument is specified, add all the known platforms into the list
			if (bAllPlatforms)
			{
				Platforms.UnionWith(UnrealTargetPlatform.GetValidPlatforms());
			}

			// Output a line for each registered platform
			foreach (UnrealTargetPlatform Platform in Platforms)
			{
				UEBuildPlatform.TryGetBuildPlatform(Platform, out UEBuildPlatform? BuildPlatform);
				string PlatformSDKString = "";
				if (bOutputSDKs)
				{
					PlatformSDKString = BuildPlatform != null ? UEBuildPlatform.GetSDK(Platform)!.GetMainVersion() : "<UNKNOWN>";
				}

				if (BuildPlatform != null && BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
				{
					Logger.LogInformation("##PlatformValidate: {Platform} VALID {PlatformSdkString}", Platform.ToString(), PlatformSDKString);
				}
				else
				{
					Logger.LogInformation("##PlatformValidate: {Platform} INVALID {PlatformSdkString}", Platform.ToString(), PlatformSDKString);
				}
			}
			return Task.FromResult(0);
		}
	}
}
