// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using Microsoft.Extensions.Configuration;

namespace EpicGames.OIDC
{
	public class ProviderConfigurationFactory
	{
		public static IConfiguration ReadConfiguration(DirectoryInfo engineDir, DirectoryInfo? gameDir)
		{
			string configFileName = "oidc-configuration.json";

			if (!engineDir.Exists)
			{
				throw new Exception($"Failed to locate engine dir at {engineDir}");
			}

			ConfigurationBuilder configBuilder = new ConfigurationBuilder();
			configBuilder
				.AddJsonFile($"{engineDir}/Programs/OidcToken/{configFileName}", true, false)
				.AddJsonFile($"{engineDir}/Restricted/NoRedist/Programs/OidcToken/{configFileName}", true, false)
				.AddJsonFile($"{engineDir}/Restricted/NotForLicensees/Programs/OidcToken/{configFileName}", true, false);

			if (gameDir?.Exists ?? false)
			{
				configBuilder.AddJsonFile($"{gameDir}/Programs/OidcToken/{configFileName}", true, false)
					.AddJsonFile($"{gameDir}/Restricted/NoRedist/Programs/OidcToken/{configFileName}", true, false)
					.AddJsonFile($"{gameDir}/Restricted/NotForLicensees/Programs/OidcToken/{configFileName}", true, false);
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