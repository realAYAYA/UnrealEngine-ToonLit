// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable IDE0005
#pragma warning disable CA1802 // warning CA1802: Field 'EnableAlerts' is declared as 'readonly' but is initialized with a constant value. Mark this field as 'const' instead.

using System.Collections.Generic;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde.Tools;

namespace UnrealGameSync
{
	/// <summary>
	/// This class contains settings for a site-specific deployment of UGS, and is read from the Deployment.json file in the application directory.
	/// </summary>
	public partial class DeploymentSettings
	{
		/// <summary>
		/// Default update source for UGS
		/// </summary>
		public LauncherUpdateSource UpdateSource { get; set; }

		/// <summary>
		/// Url for the Horde server
		/// </summary>
		public string? HordeUrl { get; set; }

		/// <summary>
		/// Identifier for the tool to sync from UGS
		/// </summary>
		public ToolId HordeToolId { get; set; } = new ToolId("ugs-win");

		/// <summary>
		/// SQL connection string used to connect to the database for telemetry and review data.
		/// </summary>
		public string? ApiUrl { get; set; }

		/// <summary>
		/// Default Perforce server to connect to
		/// </summary>
		public string? DefaultPerforceServer { get; set; }

		/// <summary>
		/// Servers to connect to for issue details by default
		/// </summary>
#pragma warning disable CA2227 // Collection properties should be read only
		public List<string> DefaultIssueApiUrls { get; set; } = new List<string>();
#pragma warning restore CA2227 // Collection properties should be read only

		/// <summary>
		/// The issue api to use for URL handler events
		/// </summary>
		public string? UrlHandleIssueApi { get; set; } = null;

		/// <summary>
		/// Specifies the depot path to sync down the stable version of UGS from, without a trailing slash (eg. //depot/UnrealGameSync/bin). This is a site-specific setting. 
		/// The UnrealGameSync executable should be located at Release/UnrealGameSync.exe under this path, with any dependent DLLs.
		/// </summary>
		public string? DefaultDepotPath { get; set; } = null;

		/// <summary>
		/// Depot path to sync additional tools from
		/// </summary>
		public string? ToolsDepotPath { get; set; } = null;

		/// <summary>
		/// DSN for sending crash reports to Sentry.
		/// </summary>
		public string? SentryDsn { get; set; } = null;

		/// <summary>
		/// Whether to allow notifications about build failures
		/// </summary>
		public bool EnableAlerts { get; set; } = true;

		static DeploymentSettings? s_instance;

		public static DeploymentSettings Instance
		{
			get
			{
				if (s_instance == null)
				{
					FileReference assemblyFile = new FileReference(Assembly.GetExecutingAssembly().Location);
					FileReference settingsFile = FileReference.Combine(assemblyFile.Directory, "Deployment.json");

					if (FileReference.Exists(settingsFile))
					{
						byte[] data = FileReference.ReadAllBytes(settingsFile);
						JsonSerializerOptions options = new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip };
						options.Converters.Add(new JsonStringEnumConverter());
						s_instance = JsonSerializer.Deserialize<DeploymentSettings>(data, options);
					}

					s_instance ??= new DeploymentSettings();
				}
				return s_instance;
			}
		}
	}
}
