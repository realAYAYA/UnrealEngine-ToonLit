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
using EpicGames.Horde.Artifacts;
using System.Linq;
using System.Diagnostics;

#nullable enable

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a CreateArtifact task
	/// </summary>
	public class CreateArtifactTaskParameters
	{
		/// <summary>
		/// Name for the artifact
		/// </summary>
		[TaskParameter]
		public string Name = String.Empty;

		/// <summary>
		/// Type of the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Type = "unknown";

		/// <summary>
		/// Description for the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Description;

		/// <summary>
		/// Base directory to resolve relative paths for input files.
		/// </summary>
		[TaskParameter]
		public string? BaseDir;

		/// <summary>
		/// Changelist number for this artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public int? Change;

		/// <summary>
		/// Files to be uploaded.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files = "...";
	}

	/// <summary>
	/// Deploys a tool update through Horde
	/// </summary>
	[TaskElement("CreateArtifact", typeof(CreateArtifactTaskParameters))]
	public class CreateArtifactTask : SpawnTaskBase
	{
		class LoggerProviderAdapter : ILoggerProvider
		{
			readonly ILogger _logger;

			public LoggerProviderAdapter(ILogger logger) => _logger = logger;
			public ILogger CreateLogger(string categoryName) => _logger;
			public void Dispose() { }
		}

		/// <summary>
		/// Parameters for this task
		/// </summary>
		CreateArtifactTaskParameters Parameters;

		/// <summary>
		/// Construct a Helm task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public CreateArtifactTask(CreateArtifactTaskParameters InParameters)
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
			// Create a DI container for building the graph
			ServiceCollection serviceCollection = new ServiceCollection();
			serviceCollection.AddHorde();
			serviceCollection.AddLogging(builder => builder.AddProvider(new LoggerProviderAdapter(Log.Logger)));
			serviceCollection.Configure<HordeOptions>(x => x.AllowAuthPrompt = !Automation.IsBuildMachine);
			serviceCollection.Configure<LoggerFilterOptions>(options => options.AddFilter(typeof(HttpClient).FullName, LogLevel.Warning));

			await using ServiceProvider serviceProvider = serviceCollection.BuildServiceProvider();

			ArtifactName artifactName = new ArtifactName(Parameters.Name);
			ArtifactType artifactType = new ArtifactType(Parameters.Type);

			HordeHttpClient hordeHttpClient = serviceProvider.GetRequiredService<HordeHttpClient>();
			int? change = (Parameters.Change == 0) ? (int?)null : Parameters.Change;
			CreateArtifactResponse response = await hordeHttpClient.CreateArtifactAsync(artifactName, artifactType, Parameters.Description, change: change);
			Logger.LogInformation("Creating artifact {ArtifactId} '{ArtifactName}' ({ArtifactType}) (ns: {NamespaceId}, ref: {RefName})", response.ArtifactId, artifactName, artifactType, response.NamespaceId, response.RefName);

			Stopwatch timer = Stopwatch.StartNew();

			HttpStorageClientFactory httpStorageClientFactory = serviceProvider.GetRequiredService<HttpStorageClientFactory>();
			using (IStorageClient client = httpStorageClientFactory.CreateClient(response.NamespaceId, response.Token))
			{
				await using (IBlobWriter writer = client.CreateBlobWriter(response.RefName))
				{
					DirectoryReference baseDir = ResolveDirectory(Parameters.BaseDir);
					List<FileInfo> files = ResolveFilespec(baseDir, Parameters.Files, TagNameToFileSet).Select(x => x.ToFileInfo()).ToList();

					int totalCount = files.Count;
					long totalSize = files.Sum(x => x.Length);

					IBlobRef<DirectoryNode> outputNodeRef = await writer.WriteFilesAsync(baseDir.ToDirectoryInfo(), files, progress: new UpdateStatsLogger(totalCount, totalSize, Logger));
					await writer.FlushAsync();

					await client.WriteRefAsync(response.RefName, outputNodeRef);
				}
			}

			Logger.LogInformation("Completed in {Time:n1}s", timer.Elapsed.TotalSeconds);
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
			return FindTagNamesFromList(Parameters.Files);
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
