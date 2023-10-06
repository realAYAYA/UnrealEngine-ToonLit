// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Primitives;
using ProtoBuf;
using StackExchange.Redis;

namespace Horde.Server.Configuration
{
	/// <summary>
	/// Service which processes runtime configuration data.
	/// </summary>
	public sealed class ConfigService : IOptionsFactory<GlobalConfig>, IOptionsChangeTokenSource<GlobalConfig>, IHostedService, IDisposable
	{
		// Index to all current config files 
		[ProtoContract]
		class ConfigSnapshot
		{
			[ProtoMember(1)]
			public byte[] Data { get; set; } = null!;

			[ProtoMember(4)]
			public Dictionary<Uri, string> Dependencies { get; set; } = new Dictionary<Uri, string>();

			[ProtoMember(5)]
			public string ServerVersion { get; set; } = String.Empty;
		}

		// Implements a token for config change notifications
		class ChangeToken : IChangeToken
		{
			class Registration : IDisposable
			{
				public Action<object>? _callback;
				public object? _state;
				public Registration? _next;

				public void Dispose() => _callback = null;
			}

			static readonly Registration s_sentinel = new Registration();

			Registration? _firstRegistration = null;

			public bool ActiveChangeCallbacks => true;
			public bool HasChanged => _firstRegistration != s_sentinel;

			public void TriggerChange()
			{
				for (Registration? firstRegistration = _firstRegistration; firstRegistration != s_sentinel; firstRegistration = _firstRegistration)
				{
					if (Interlocked.CompareExchange(ref _firstRegistration, s_sentinel, firstRegistration) == firstRegistration)
					{
						for (; firstRegistration != null; firstRegistration = firstRegistration._next)
						{
							Action<object>? callback = firstRegistration._callback;
							if (callback != null)
							{
								callback(firstRegistration._state!);
							}
						}
						break;
					}
				}
			}

			public IDisposable RegisterChangeCallback(Action<object> callback, object state)
			{
				Registration registration = new Registration { _callback = callback, _state = state };
				for (; ; )
				{
					Registration? nextRegistration = _firstRegistration;
					registration._next = nextRegistration;

					if (nextRegistration == s_sentinel)
					{
						callback(state);
						break;
					}
					if (Interlocked.CompareExchange(ref _firstRegistration, registration, nextRegistration) == nextRegistration)
					{
						break;
					}
				}
				return registration;
			}
		}

		// The current config
		record class ConfigState(IoHash Hash, GlobalConfig GlobalConfig);

		readonly RedisService _redisService;
		readonly ServerSettings _serverSettings;
		readonly Dictionary<string, IConfigSource> _sources;
		readonly JsonSerializerOptions _jsonOptions;
		readonly RedisKey _snapshotKey = "config";
		readonly ILogger _logger;

		readonly ITicker _ticker;
		readonly TimeSpan _tickInterval = TimeSpan.FromMinutes(1.0);

		readonly RedisChannel _updateChannel = "config-update";
		readonly BackgroundTask _updateTask;

		Task<ConfigState> _stateTask;
		ChangeToken? _currentChangeToken;

		/// <inheritdoc/>
		string IOptionsChangeTokenSource<GlobalConfig>.Name => String.Empty;

		/// <summary>
		/// Event for notifications that the config has been updated
		/// </summary>
		public event Action<Exception?>? OnConfigUpdate;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConfigService(RedisService redisService, IOptions<ServerSettings> serverSettings, IEnumerable<IConfigSource> sources, IClock clock, ILogger<ConfigService> logger)
		{
			_redisService = redisService;
			_serverSettings = serverSettings.Value;
			_sources = sources.ToDictionary(x => x.Scheme, x => x, StringComparer.OrdinalIgnoreCase);
			_logger = logger;

			_jsonOptions = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(_jsonOptions);

			_ticker = clock.AddSharedTicker<ConfigService>(_tickInterval, TickSharedAsync, logger);

			_updateTask = new BackgroundTask(WaitForUpdatesAsync);

			_stateTask = Task.Run(() => GetStartupStateAsync());
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_updateTask.Dispose();
			_ticker.Dispose();
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			if (_serverSettings.IsRunModeActive(RunMode.Worker))
			{
				await _ticker.StartAsync();
			}
			_updateTask.Start();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _updateTask.StopAsync();
			if (_serverSettings.IsRunModeActive(RunMode.Worker))
			{
				await _ticker.StopAsync();
			}
		}

		/// <summary>
		/// Accessor for tests to update the global config
		/// </summary>
		/// <param name="hash">Hash for the config data</param>
		/// <param name="globalConfig">New config value</param>
		public void Set(IoHash hash, GlobalConfig globalConfig)
		{
			// Set the new state
			_stateTask = Task.FromResult(new ConfigState(hash, globalConfig));

			// Notify any watchers of the new config object
			for (; ; )
			{
				ChangeToken? token = _currentChangeToken;
				if (Interlocked.CompareExchange(ref _currentChangeToken, null, token) == token)
				{
					token?.TriggerChange();
					break;
				}
			}
		}

		/// <inheritdoc/>
		public GlobalConfig Create(string name) => _stateTask.Result.GlobalConfig;

		/// <inheritdoc/>
		public IChangeToken GetChangeToken()
		{
			ChangeToken? newToken = null;
			for (; ; )
			{
				ChangeToken? token = _currentChangeToken;
				if (token != null)
				{
					return token;
				}

				newToken ??= new ChangeToken();

				if (Interlocked.CompareExchange(ref _currentChangeToken, newToken, null) == null)
				{
					return newToken;
				}
			}
		}

		async Task<ConfigState> GetStartupStateAsync()
		{
			ReadOnlyMemory<byte> data = ReadOnlyMemory<byte>.Empty;
			if (!_serverSettings.ForceConfigUpdateOnStartup)
			{
				data = await ReadSnapshotDataAsync();
			}
			if (data.Length == 0)
			{
				ConfigSnapshot snapshot = await CreateSnapshotAsync(CancellationToken.None);
				await WriteSnapshotAsync(snapshot);
				data = SerializeSnapshot(snapshot);
			}
			return new ConfigState(IoHash.Compute(data.Span), CreateGlobalConfig(data));
		}

		async ValueTask TickSharedAsync(CancellationToken cancellationToken)
		{
			// Read a new copy of the current snapshot in the context of being elected to perform updates. This
			// avoids any latency with receiving notifications on the pub/sub channel about changes.
			ReadOnlyMemory<byte> initialData = await ReadSnapshotDataAsync();

			// Print info about the initial snapshot
			ConfigSnapshot? snapshot = null;
			if (initialData.Length > 0)
			{
				snapshot = DeserializeSnapshot(initialData);
				_logger.LogDebug("Initial snapshot for update: {@Info}", GetSnapshotInfo(initialData.Span, snapshot));
			}

			// Update the snapshot until we're asked to stop
			while (!cancellationToken.IsCancellationRequested)
			{
				if (snapshot == null || await IsOutOfDateAsync(snapshot, cancellationToken))
				{
					snapshot = await CreateSnapshotGuardedAsync(cancellationToken);
					if (snapshot != null)
					{
						try
						{
							await WriteSnapshotAsync(snapshot);
						}
						catch (Exception ex)
						{
							_logger.LogError(ex, "Error while updating config: {Message}", ex.Message);
						}
					}
				}
				await Task.Delay(_tickInterval, cancellationToken);
			}
		}

		async Task WaitForUpdatesAsync(CancellationToken cancellationToken)
		{
			AsyncEvent updateEvent = new AsyncEvent();
			await using (RedisSubscription subscription = await _redisService.SubscribeAsync(_updateChannel, _ => updateEvent.Pulse()))
			{
				Task cancellationTask = Task.Delay(-1, cancellationToken);
				while (!cancellationToken.IsCancellationRequested)
				{
					Task updateTask = updateEvent.Task;

					try
					{
						RedisValue value = await _redisService.GetDatabase().StringGetAsync(_snapshotKey);
						if (!value.IsNullOrEmpty)
						{
							await UpdateConfigObjectAsync(value);
						}
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Exception while updating config from Redis: {Message}", ex.Message);
						await Task.Delay(TimeSpan.FromMinutes(1.0), cancellationToken);
						continue;
					}

					await await Task.WhenAny(updateTask, cancellationTask);
				}
			}
		}

		/// <summary>
		/// Create a new snapshot object, catching any exceptions and sending notifications
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New config snapshot</returns>
		async Task<ConfigSnapshot?> CreateSnapshotGuardedAsync(CancellationToken cancellationToken)
		{
			try
			{
				ConfigSnapshot snapshot = await CreateSnapshotAsync(cancellationToken);
				OnConfigUpdate?.Invoke(null);
				return snapshot;
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while updating config: {Message}", ex.Message);
				OnConfigUpdate?.Invoke(ex);
				return null;
			}
		}

		/// <summary>
		/// Create a new snapshot object
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New config snapshot</returns>
		async Task<ConfigSnapshot> CreateSnapshotAsync(CancellationToken cancellationToken)
		{
			// Get the path to the root config file
			Uri globalConfigUri;
			if (Path.IsPathRooted(_serverSettings.ConfigPath) && !_serverSettings.ConfigPath.StartsWith("//", StringComparison.Ordinal))
			{
				// absolute path to config
				globalConfigUri = new Uri(_serverSettings.ConfigPath);
			}
			else
			{
				// relative (development) or perforce path
				globalConfigUri = ConfigType.CombinePaths(new Uri(FileReference.Combine(Program.AppDir, "_").FullName), _serverSettings.ConfigPath);
			}

			// Read the config files
			ConfigContext context = new ConfigContext(_jsonOptions, _sources, _logger);
			try
			{
				ConfigSnapshot snapshot = new ConfigSnapshot();
				snapshot.ServerVersion = Program.Version.ToString();

				// Read the new config in
				GlobalConfig globalConfig = await ConfigType.ReadAsync<GlobalConfig>(globalConfigUri, context, cancellationToken);

				// Serialize it back out to a byte array
				snapshot.Data = JsonSerializer.SerializeToUtf8Bytes(globalConfig, context.JsonOptions);

				// Save all the dependencies
				foreach ((Uri depUri, IConfigFile depFile) in context.Files)
				{
					snapshot.Dependencies.Add(depUri, depFile.Revision);
				}

				return snapshot;
			}
			catch (Exception ex) when (ex is not ConfigException)
			{
				throw new ConfigException(context, ex.Message, ex);
			}
		}

		internal byte[] Serialize(GlobalConfig config)
		{
			return JsonSerializer.SerializeToUtf8Bytes(config, _jsonOptions);
		}
		
		internal GlobalConfig? Deserialize(byte[] data)
		{
			return JsonSerializer.Deserialize<GlobalConfig>(data, _jsonOptions);
		}

		/// <summary>
		/// Writes information about the current snapshot to the logger
		/// </summary>
		/// <param name="span">Data for the snapshot</param>
		/// <param name="snapshot">Snapshot to log information on</param>
		static object GetSnapshotInfo(ReadOnlySpan<byte> span, ConfigSnapshot snapshot)
		{
			return new { Revision = IoHash.Compute(span).ToString(), ServerVersion = snapshot.ServerVersion.ToString(), Dependencies = snapshot.Dependencies.ToArray() };
		}

		/// <summary>
		/// Checks if the given snapshot is out of date
		/// </summary>
		/// <param name="snapshot">The current config snapshot</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the snapshot is out of date</returns>
		async Task<bool> IsOutOfDateAsync(ConfigSnapshot snapshot, CancellationToken cancellationToken)
		{
			// Always re-read the config file when switching server versions
			string newServerVersion = Program.Version.ToString();
			if (!snapshot.ServerVersion.Equals(newServerVersion, StringComparison.Ordinal))
			{
				_logger.LogInformation("Config is out of date (server version {OldVersion} -> {NewVersion})", snapshot.ServerVersion, newServerVersion);
				return true;
			}

			// Group the dependencies by scheme in order to allow the source to batch-query them
			foreach(IGrouping<string, KeyValuePair<Uri, string>> group in snapshot.Dependencies.GroupBy(x => x.Key.Scheme))
			{
				KeyValuePair<Uri, string>[] pairs = group.ToArray();

				IConfigSource? source;
				if (!_sources.TryGetValue(group.Key, out source))
				{
					_logger.LogInformation("Config is out of date (missing source '{Source}')", group.Key);
					return true;
				}

				IConfigFile[] files = await source.GetAsync(pairs.ConvertAll(x => x.Key), cancellationToken);
				for (int idx = 0; idx < pairs.Length; idx++)
				{
					if (!files[idx].Revision.Equals(pairs[idx].Value, StringComparison.Ordinal))
					{
						_logger.LogInformation("Config is out of date (file {Path}: {OldVersion} -> {NewVersion})", files[idx].Uri, pairs[idx].Value, files[idx].Revision);
						return true;
					}
				}
			}
			return false;
		}

		async Task WriteSnapshotAsync(ConfigSnapshot snapshot)
		{
			ReadOnlyMemory<byte> data = SerializeSnapshot(snapshot);
			IoHash hash = IoHash.Compute(data.Span);

			await _redisService.GetDatabase().StringSetAsync(_snapshotKey, data);
			await _redisService.PublishAsync(_updateChannel, RedisValue.EmptyString);

			_logger.LogInformation("Published new config snapshot (hash: {Hash}, server: {Server}, size: {Size})", hash.ToString(), Program.Version.ToString(), data.Length);
		}

		async Task<ReadOnlyMemory<byte>> ReadSnapshotDataAsync()
		{
			RedisValue value = await _redisService.GetDatabase().StringGetAsync(_snapshotKey);
			return value.IsNullOrEmpty ? ReadOnlyMemory<byte>.Empty : (ReadOnlyMemory<byte>)value;
		}

		static ReadOnlyMemory<byte> SerializeSnapshot(ConfigSnapshot snapshot)
		{
			using (MemoryStream outputStream = new MemoryStream())
			{
				using GZipStream zipStream = new GZipStream(outputStream, CompressionMode.Compress, false);
				ProtoBuf.Serializer.Serialize(zipStream, snapshot);
				zipStream.Flush();
				return outputStream.ToArray();
			}
		}

		static ConfigSnapshot DeserializeSnapshot(ReadOnlyMemory<byte> data)
		{
			using (ReadOnlyMemoryStream outputStream = new ReadOnlyMemoryStream(data))
			{
				using GZipStream zipStream = new GZipStream(outputStream, CompressionMode.Decompress, false);
				return ProtoBuf.Serializer.Deserialize<ConfigSnapshot>(zipStream);
			}
		}

		async Task UpdateConfigObjectAsync(ReadOnlyMemory<byte> data)
		{
			ConfigState state = await _stateTask;

			// Hash the data and only update if it changes; this prevents any double-updates due to time between initialization and the hosted service starting.
			IoHash hash = IoHash.Compute(data.Span);
			if (hash != state.Hash)
			{
				_logger.LogInformation("Updating config state from Redis (hash: {OldHash} -> {NewHash}, new size: {Size})", state.Hash, hash, data.Length);
				GlobalConfig globalConfig = CreateGlobalConfig(data);
				Set(hash, globalConfig);
			}
		}

		GlobalConfig CreateGlobalConfig(ReadOnlyMemory<byte> data)
		{
			// Decompress the snapshot object
			ConfigSnapshot snapshot = DeserializeSnapshot(data);

			// Build the new config and store the project and stream configs inside it
			GlobalConfig globalConfig = JsonSerializer.Deserialize<GlobalConfig>(snapshot.Data, _jsonOptions)!;
			globalConfig.Revision = IoHash.Compute(data.Span).ToString();

			// Compute hashes for all the stream objects
			foreach (ProjectConfig projectConfig in globalConfig.Projects)
			{
				foreach (StreamConfig streamConfig in projectConfig.Streams)
				{
					byte[] streamData = JsonSerializer.SerializeToUtf8Bytes(streamConfig, _jsonOptions);
					streamConfig.Revision = IoHash.Compute(streamData).ToString();
				}
			}

			// Run the postload callbacks on all the config objects
			globalConfig.PostLoad(_serverSettings);
			return globalConfig;
		}
	}
}
