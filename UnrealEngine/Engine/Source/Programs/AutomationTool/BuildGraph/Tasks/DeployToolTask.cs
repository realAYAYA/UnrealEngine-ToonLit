// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Text.Json;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using System.Threading;
using System.Data;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.DependencyInjection;
using EpicGames.Horde;
using EpicGames.Horde.Tools;
using EpicGames.Horde.Server;

#nullable enable

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a DeployTool task
	/// </summary>
	public class DeployToolTaskParameters
	{
		/// <summary>
		/// Identifier for the tool
		/// </summary>
		[TaskParameter]
		public string Id = String.Empty;

		/// <summary>
		/// Settings file to use for the deployment. Should be a JSON file containing server name and access token.
		/// </summary>
		[TaskParameter]
		public string Settings = String.Empty;

		/// <summary>
		/// Version number for the new tool
		/// </summary>
		[TaskParameter]
		public string Version = String.Empty;

		/// <summary>
		/// Duration over which to roll out the tool, in minutes.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Duration = 0;

		/// <summary>
		/// Whether to create the deployment as paused
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Paused = false;

		/// <summary>
		/// Zip file containing files to upload
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? File = null!;

		/// <summary>
		/// Directory to upload for the tool
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Directory = null!;
	}

	/// <summary>
	/// Deploys a tool update through Horde
	/// </summary>
	[TaskElement("DeployTool", typeof(DeployToolTaskParameters))]
	public class DeployToolTask : SpawnTaskBase
	{
		class DeploySettings
		{
			public string Server { get; set; } = String.Empty;
			public string? Token { get; set; }
		}

		/// <summary>
		/// Options for a new deployment
		/// </summary>
		class CreateDeploymentRequest
		{
			public string Version { get; set; } = "Unknown";
			public double? Duration { get; set; }
			public bool? CreatePaused { get; set; }
			public string? Node { get; set; }
		}

		/// <summary>
		/// Parameters for this task
		/// </summary>
		DeployToolTaskParameters Parameters;

		/// <summary>
		/// Construct a Helm task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public DeployToolTask(DeployToolTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			FileReference settingsFile = ResolveFile(Parameters.Settings);
			if (!FileReference.Exists(settingsFile))
			{
				throw new AutomationException($"Settings file '{settingsFile}' does not exist");
			}

			byte[] settingsData = await FileReference.ReadAllBytesAsync(settingsFile);
			JsonSerializerOptions jsonOptions = new JsonSerializerOptions { AllowTrailingCommas = true, ReadCommentHandling = JsonCommentHandling.Skip, PropertyNameCaseInsensitive = true };

			DeploySettings? settings = JsonSerializer.Deserialize<DeploySettings>(settingsData, jsonOptions);
			if (settings == null)
			{
				throw new AutomationException($"Unable to read settings file {settingsFile}");
			}
			else if (settings.Server == null)
			{
				throw new AutomationException($"Missing 'server' key from {settingsFile}");
			}

			ToolId toolId = new ToolId(Parameters.Id);

			ServiceCollection serviceCollection = new ServiceCollection();
			serviceCollection.Configure<HordeOptions>(options =>
			{
				options.ServerUrl = new Uri(settings.Server);
				options.AccessToken = settings.Token;
			});
			serviceCollection.AddHttpClient();
			serviceCollection.AddHorde();

			await using ServiceProvider serviceProvider = serviceCollection.BuildServiceProvider();
			IHordeClient hordeClient = serviceProvider.GetRequiredService<IHordeClient>();

			using HordeHttpClient hordeHttpClient = hordeClient.CreateHttpClient();

			GetServerInfoResponse infoResponse = await hordeHttpClient.GetServerInfoAsync();
			Logger.LogInformation("Uploading {ToolId} to {ServerUrl} (Version: {Version}, API v{ApiVersion})...", toolId, settings.Server, infoResponse.ServerVersion, (int)infoResponse.ApiVersion);

			BlobSerializerOptions serializerOptions = BlobSerializerOptions.Create(infoResponse.ApiVersion);

			IBlobRef handle;

			using IStorageClient storageClient = hordeClient.CreateStorageClient(toolId);
			await using (IBlobWriter blobWriter = storageClient.CreateBlobWriter(serializerOptions: serializerOptions))
			{
				DirectoryNode sandbox = new DirectoryNode();
				if (Parameters.File != null)
				{
					using FileStream stream = FileReference.Open(ResolveFile(Parameters.File), FileMode.Open, FileAccess.Read);
					await sandbox.CopyFromZipStreamAsync(stream, blobWriter, new ChunkingOptions());
				}
				else if (Parameters.Directory != null)
				{
					DirectoryInfo directoryInfo = ResolveDirectory(Parameters.Directory).ToDirectoryInfo();
					await sandbox.AddFilesAsync(directoryInfo, blobWriter);
				}
				else
				{
					throw new AutomationException("Either File=... or Directory=... must be specified");
				}
				handle = await blobWriter.WriteBlobAsync(sandbox);
				await blobWriter.FlushAsync();
			}

			double? duration = null;
			if (Parameters.Duration != 0)
			{
				duration = Parameters.Duration;
			}

			bool? createPaused = null;
			if (Parameters.Paused)
			{
				createPaused = true;
			}

			BlobRefValue locator = handle.GetRefValue();
			ToolDeploymentId deploymentId = await hordeHttpClient.CreateToolDeploymentAsync(toolId, Parameters.Version, duration, createPaused, locator);
			Logger.LogInformation("Created {ToolId} deployment {DeploymentId}", toolId, deploymentId);
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
