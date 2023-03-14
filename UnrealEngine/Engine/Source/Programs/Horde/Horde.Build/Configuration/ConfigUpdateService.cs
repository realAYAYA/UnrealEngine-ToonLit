// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Notifications;
using Horde.Build.Perforce;
using Horde.Build.Projects;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Tools;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Configuration
{
	using PoolId = StringId<IPool>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Polls Perforce for stream config changes
	/// </summary>
	public sealed class ConfigUpdateService : IHostedService, IDisposable
	{
		const string FileScheme = "file";
		const string PerforceScheme = "p4-cluster";

		/// <summary>
		/// Config file version number
		/// </summary>
		const int Version = 12;

		readonly MongoService _mongoService;
		readonly ConfigCollection _configCollection;
		readonly ToolCollection _toolCollection;
		readonly ProjectService _projectService;
		readonly StreamService _streamService;
		readonly IPerforceService _perforceService;
		readonly INotificationService _notificationService;
		readonly AgentService _agentService;
		readonly PoolService _poolService;
		readonly IOptionsMonitor<ServerSettings> _settings;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConfigUpdateService(MongoService mongoService, ConfigCollection configCollection, IPerforceService perforceService, ToolCollection toolCollection, ProjectService projectService, StreamService streamService, INotificationService notificationService, PoolService poolService, AgentService agentService, IClock clock, IOptionsMonitor<ServerSettings> settings, ILogger<ConfigUpdateService> logger)
		{
			_mongoService = mongoService;
			_configCollection = configCollection;
			_perforceService = perforceService;
			_toolCollection = toolCollection;
			_projectService = projectService;
			_streamService = streamService;
			_notificationService = notificationService;
			_poolService = poolService;
			_agentService = agentService;
			_settings = settings;
			if (mongoService.ReadOnlyMode)
			{
				_ticker = new NullTicker();
			}
			else
			{
				_ticker = clock.AddSharedTicker<ConfigUpdateService>(TimeSpan.FromMinutes(1.0), TickLeaderAsync, logger);
			}
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		GlobalConfig? _cachedGlobalConfig;
		string? _cachedGlobalConfigRevision;
		Dictionary<ProjectId, (ProjectConfig Config, string Revision)> _cachedProjectConfigs = new Dictionary<ProjectId, (ProjectConfig, string)>();
		readonly Dictionary<ProjectId, string?> _cachedLogoRevisions = new Dictionary<ProjectId, string?>();

		async Task UpdateConfigAsync(Uri configPath)
		{
			// Update the globals singleton
			GlobalConfig globalConfig;
			for (; ; )
			{
				Dictionary<Uri, string> globalRevisions = await FindRevisionsAsync(new[] { configPath });
				if (globalRevisions.Count == 0)
				{
					throw new Exception($"Invalid config path: {configPath}");
				}

				string revision = globalRevisions.First().Value;
				if (_cachedGlobalConfig == null || revision != _cachedGlobalConfigRevision)
				{
					_logger.LogInformation("Caching global config from {Revision}", revision);
					try
					{
						_cachedGlobalConfig = await ReadDataAsync<GlobalConfig>(revision, configPath);
						_cachedGlobalConfigRevision = revision;
					}
					catch (Exception ex)
					{
						await SendFailureNotificationAsync(ex, configPath);
						return;
					}
				}
				globalConfig = _cachedGlobalConfig;

				Globals globals = await _mongoService.GetGlobalsAsync();
				if (globals.ConfigRevision == revision)
				{
					break;
				}

				_logger.LogInformation("Updating configuration from {ConfigPath}", globals.ConfigRevision);

				globals.ConfigRevision = revision;
				globals.PerforceClusters = _cachedGlobalConfig.PerforceClusters;
				globals.ScheduledDowntime = _cachedGlobalConfig.Downtime;
				globals.MaxConformCount = _cachedGlobalConfig.MaxConformCount;
				globals.ComputeClusters = _cachedGlobalConfig.Compute;
				globals.RootAcl = Acl.Merge(null, _cachedGlobalConfig.Acl);

				if (await _mongoService.TryUpdateSingletonAsync(globals))
				{
					break;
				}
			}

			// Update the agent rate table
			await _agentService.UpdateRateTableAsync(globalConfig.Rates);

			// Update the tools
			await _toolCollection.ConfigureAsync(globalConfig.Tools);

			// Projects to remove
			List<IProject> projects = await _projectService.GetProjectsAsync();

			// Get the path to all the project configs
			List<(ProjectConfigRef ProjectRef, Uri Path)> projectConfigs = globalConfig.Projects.Select(x => (x, CombinePaths(configPath, x.Path))).ToList();

			Dictionary<ProjectId, (ProjectConfig Config, string Revision)> prevCachedProjectConfigs = _cachedProjectConfigs;
			_cachedProjectConfigs = new Dictionary<ProjectId, (ProjectConfig, string)>();

			List<(ProjectId ProjectId, Uri Path)> projectLogos = new List<(ProjectId ProjectId, Uri Path)>();

			List<(ProjectId ProjectId, StreamConfigRef StreamRef, Uri Path)> streamConfigs = new List<(ProjectId, StreamConfigRef, Uri)>();

			// List of project ids that were not able to be updated. We will avoid removing any existing project or stream definitions for these.
			HashSet<ProjectId> skipProjectIds = new HashSet<ProjectId>();

			// Update any existing projects
			Dictionary<Uri, string> projectRevisions = await FindRevisionsAsync(projectConfigs.Select(x => x.Path));
			for (int idx = 0; idx < projectConfigs.Count; idx++)
			{
				// Make sure we were able to fetch metadata for 
				(ProjectConfigRef projectRef, Uri projectPath) = projectConfigs[idx];
				if (!projectRevisions.TryGetValue(projectPath, out string? revision))
				{
					_logger.LogWarning("Unable to update project {ProjectId} due to missing revision information", projectRef.Id);
					skipProjectIds.Add(projectRef.Id);
					continue;
				}

				IProject? project = projects.FirstOrDefault(x => x.Id == projectRef.Id);
				bool update = project == null || project.ConfigPath != projectPath.ToString() || project.ConfigRevision != revision;

				ProjectConfig? projectConfig;
				if (!update && prevCachedProjectConfigs.TryGetValue(projectRef.Id, out (ProjectConfig Config, string Revision) result) && result.Revision == revision)
				{
					projectConfig = result.Config;
				}
				else
				{
					_logger.LogInformation("Caching configuration for project {ProjectId} ({Revision})", projectRef.Id, revision);
					try
					{
						projectConfig = await ReadDataAsync<ProjectConfig>(revision, projectPath);
						if (update)
						{
							_logger.LogInformation("Updating configuration for project {ProjectId} ({Revision})", projectRef.Id, revision);
							await _projectService.Collection.AddOrUpdateAsync(projectRef.Id, projectPath.ToString(), revision, idx, projectConfig);
						}
					}
					catch (Exception ex)
					{
						await SendFailureNotificationAsync(ex, projectPath);
						skipProjectIds.Add(projectRef.Id);
						continue;
					}
				}

				if (projectConfig.Logo != null)
				{
					projectLogos.Add((projectRef.Id, CombinePaths(projectPath, projectConfig.Logo)));
				}

				_cachedProjectConfigs[projectRef.Id] = (projectConfig, revision);
				streamConfigs.AddRange(projectConfig.Streams.Select(x => (projectRef.Id, x, CombinePaths(projectPath, x.Path))));
			}

			// Get the logo revisions
			Dictionary<Uri, string> logoRevisions = await FindRevisionsAsync(projectLogos.Select(x => x.Path));
			for (int idx = 0; idx < projectLogos.Count; idx++)
			{
				(ProjectId projectId, Uri path) = projectLogos[idx];
				if (logoRevisions.TryGetValue(path, out string? revision))
				{
					string? currentRevision;
					if (!_cachedLogoRevisions.TryGetValue(projectId, out currentRevision))
					{
						currentRevision = (await _projectService.Collection.GetLogoAsync(projectId))?.Revision;
						_cachedLogoRevisions[projectId] = currentRevision;
					}
					if (revision != currentRevision)
					{
						_logger.LogInformation("Updating logo for project {ProjectId} ({Revision})", projectId, revision);
						try
						{
							await _projectService.Collection.SetLogoAsync(projectId, path.ToString(), revision, GetMimeTypeFromPath(path), await ReadDataAsync(path));
							_cachedLogoRevisions[projectId] = revision;
						}
						catch (Exception ex)
						{
							await SendFailureNotificationAsync(ex, path);
							continue;
						}
					}
				}
			}

			// Get the current streams
			List<IStream> streams = await _streamService.GetStreamsAsync();

			// Get the revisions for all the stream documents
			Dictionary<Uri, string> streamRevisions = await FindRevisionsAsync(streamConfigs.Select(x => x.Path));
			for (int idx = 0; idx < streamConfigs.Count; idx++)
			{
				(ProjectId projectId, StreamConfigRef streamRef, Uri streamPath) = streamConfigs[idx];
				if (streamRevisions.TryGetValue(streamPath, out string? revision))
				{
					IStream? stream = streams.FirstOrDefault(x => x.Id == streamRef.Id);
					if (stream == null || stream.ConfigRevision != revision)
					{
						_logger.LogInformation("Updating configuration for stream {StreamRef} ({Revision})", streamRef.Id, revision);
						try
						{
							StreamConfig streamConfig = await ReadDataAsync<StreamConfig>(revision, streamPath);
							stream = await _streamService.StreamCollection.CreateOrReplaceAsync(streamRef.Id, stream, revision, projectId);
						}
						catch (Exception ex)
						{
							await SendFailureNotificationAsync(ex, streamPath);
							continue;
						}
					}
				}
			}

			// Remove any projects which are no longer used
			HashSet<ProjectId> removeProjectIds = new HashSet<ProjectId>(projects.Select(x => x.Id));
			removeProjectIds.ExceptWith(projectConfigs.Select(y => y.ProjectRef.Id));

			foreach (ProjectId removeProjectId in removeProjectIds)
			{
				_logger.LogInformation("Removing project {ProjectId}", removeProjectId);
				await _projectService.DeleteProjectAsync(removeProjectId);
			}

			// Remove any streams that are no longer used
			HashSet<StreamId> removeStreamIds = new HashSet<StreamId>(streams.Where(x => !skipProjectIds.Contains(x.ProjectId)).Select(x => x.Id));
			removeStreamIds.ExceptWith(streamConfigs.Select(x => x.StreamRef.Id));

			foreach (StreamId removeStreamId in removeStreamIds)
			{
				_logger.LogInformation("Removing stream {StreamId}", removeStreamId);
				await _streamService.DeleteStreamAsync(removeStreamId);
			}
		}

		static readonly FileExtensionContentTypeProvider s_contentTypeProvider = new FileExtensionContentTypeProvider();

		static string GetMimeTypeFromPath(Uri path)
		{
			string? contentType;
			if (!s_contentTypeProvider.TryGetContentType(path.AbsolutePath, out contentType))
			{
				contentType = "application/octet-stream";
			}
			return contentType;
		}

		static Uri CombinePaths(Uri baseUri, string path)
		{
			if (path.StartsWith("//", StringComparison.Ordinal))
			{
				if (baseUri.Scheme == PerforceScheme)
				{
					return new Uri($"{PerforceScheme}://{baseUri.Host}{path}");
				}
				else
				{
					return new Uri($"{PerforceScheme}://{PerforceCluster.DefaultName}{path}");
				}
			}
			return new Uri(baseUri, path);
		}

		async Task<Dictionary<Uri, string>> FindRevisionsAsync(IEnumerable<Uri> paths)
		{
			Dictionary<Uri, string> revisions = new Dictionary<Uri, string>();

			// Find all the Perforce uris
			List<Uri> perforcePaths = new List<Uri>();
			foreach (Uri path in paths)
			{
				if (path.Scheme == FileScheme)
				{
					revisions[path] = $"ver={Version},md5={ContentHash.MD5(new FileReference(path.LocalPath))}";
				}
				else if (path.Scheme == PerforceScheme)
				{
					perforcePaths.Add(path);
				}
				else
				{
					throw new Exception($"Invalid path format: {path}");
				}
			}

			// Query all the Perforce revisions
			foreach (IGrouping<string, Uri> perforcePath in perforcePaths.GroupBy(x => x.Host, StringComparer.OrdinalIgnoreCase))
			{
				List<FileSummary> files = await _perforceService.FindFilesAsync(perforcePath.Key, perforcePath.Select(x => x.AbsolutePath));
				foreach (FileSummary file in files)
				{
					Uri fileUri = new Uri($"{PerforceScheme}://{perforcePath.Key}{file.DepotPath}");
					if (file.Error == null)
					{
						revisions[fileUri] = $"ver={Version},chg={file.Change},path={fileUri}";
					}
					else
					{
						_notificationService.NotifyConfigUpdateFailure(file.Error, file.DepotPath);
					}
				}
			}

			return revisions;
		}

		async Task<T> ReadDataAsync<T>(string revision, Uri configPath) where T : class
		{
			byte[] data = await ReadDataAsync(configPath);
			await _configCollection.AddConfigDataAsync(revision, data);
			return await _configCollection.GetConfigAsync<T>(revision);
		}

		Task<byte[]> ReadDataAsync(Uri configPath)
		{
			switch (configPath.Scheme)
			{
				case FileScheme:
					return File.ReadAllBytesAsync(configPath.LocalPath);
				case PerforceScheme:
					return _perforceService.PrintAsync(configPath.Host, configPath.AbsolutePath);
				default:
					throw new Exception($"Invalid config path: {configPath}");
			}
		}

		async Task SendFailureNotificationAsync(Exception ex, Uri configPath)
		{
			_logger.LogError(ex, "Unable to read data from {ConfigPath}: {Message}", configPath, ex.Message);

			string fileName = configPath.AbsolutePath;
			int change = -1;
			IUser? author = null;
			string? description = null;

			if (configPath.Scheme == PerforceScheme)
			{
				try
				{
					List<FileSummary> files = await _perforceService.FindFilesAsync(configPath.Host, new[] { fileName });
					change = files[0].Change;

					List<ChangeSummary> changes = await _perforceService.GetChangesAsync(configPath.Host, change, change, 1);
					if (changes.Count > 0 && changes[0].Number == change)
					{
						(author, description) = (changes[0].Author, changes[0].Description);
					}
				}
				catch (Exception ex2)
				{
					_logger.LogError(ex2, "Unable to identify change that last modified {ConfigPath} from Perforce", configPath);
				}
			}

			_notificationService.NotifyConfigUpdateFailure(ex.Message, fileName, change, author, description);
		}

		/// <inheritdoc/>
		async ValueTask TickLeaderAsync(CancellationToken stoppingToken)
		{

			Uri? configUri = null;

			if (Path.IsPathRooted(_settings.CurrentValue.ConfigPath) && !_settings.CurrentValue.ConfigPath.StartsWith("//", StringComparison.Ordinal))
			{
				// absolute path to config
				configUri = new Uri(_settings.CurrentValue.ConfigPath);
			}
			else if (_settings.CurrentValue.ConfigPath != null)
			{
				// relative (development) or perforce path
				configUri = CombinePaths(new Uri(FileReference.Combine(Program.AppDir, "_").FullName), _settings.CurrentValue.ConfigPath);
			}

			if (configUri != null)
			{
				await UpdateConfigAsync(configUri);
			}
		}
	}
}
