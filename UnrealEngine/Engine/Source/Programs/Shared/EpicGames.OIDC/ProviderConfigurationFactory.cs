// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using Microsoft.Extensions.Configuration;

#pragma warning disable CS1591 // Missing XML documentation on public types

namespace EpicGames.OIDC
{
	public static class ProviderConfigurationFactory
	{
		const string ConfigFileName = "oidc-configuration.json";

		public static IReadOnlyList<string> ConfigPaths { get; } = new string[]
		{
			$"Programs/OidcToken/{ConfigFileName}",
			$"Restricted/NoRedist/Programs/OidcToken/{ConfigFileName}",
			$"Restricted/NotForLicensees/Programs/OidcToken/{ConfigFileName}"
		};

		public static IConfiguration ReadConfiguration(DirectoryInfo engineDir, DirectoryInfo? gameDir)
		{

			if (!engineDir.Exists)
			{
				throw new Exception($"Failed to locate engine dir at {engineDir}");
			}

			ConfigurationBuilder configBuilder = new ConfigurationBuilder();
			configBuilder
				.AddJsonFile($"{engineDir}/Programs/OidcToken/{ConfigFileName}", true, false)
				.AddJsonFile($"{engineDir}/Restricted/NoRedist/Programs/OidcToken/{ConfigFileName}", true, false)
				.AddJsonFile($"{engineDir}/Restricted/NotForLicensees/Programs/OidcToken/{ConfigFileName}", true, false);

			if (gameDir?.Exists ?? false)
			{
				configBuilder.AddJsonFile($"{gameDir}/Programs/OidcToken/{ConfigFileName}", true, false)
					.AddJsonFile($"{gameDir}/Restricted/NoRedist/Programs/OidcToken/{ConfigFileName}", true, false)
					.AddJsonFile($"{gameDir}/Restricted/NotForLicensees/Programs/OidcToken/{ConfigFileName}", true, false);
			}

			IConfiguration config = configBuilder.Build();
			return config.GetSection("OidcToken");
		}

		public static IConfiguration MergeConfiguration(IEnumerable<(DirectoryInfo, DirectoryInfo?)> configurationPaths)
		{
			ConfigurationBuilder builder = new ConfigurationBuilder();
			foreach ((DirectoryInfo engineDir, DirectoryInfo? gameDir) in configurationPaths)
			{
				IConfiguration newConfiguration = ReadConfiguration(engineDir, gameDir);
				builder.AddConfiguration(newConfiguration);
			}

			return builder.Build();
		}
	}
}