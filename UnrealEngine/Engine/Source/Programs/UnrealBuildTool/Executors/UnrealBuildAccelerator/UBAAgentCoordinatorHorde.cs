// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class UBAHordeSession : IAsyncDisposable
	{
		/// <summary>
		/// ID for this Horde session
		/// </summary>
		readonly Guid _id = Guid.NewGuid();

		const string ResourceLogicalCores = "LogicalCores";
		readonly ClusterId _clusterId = new("default");

		readonly Uri _hordeUri;
		readonly string? _pool;
		readonly bool _allowWine;
		readonly int _maxCores;
		readonly bool _strict;
		readonly ConnectionMode? _connectionMode;
		readonly Encryption? _encryption;
		readonly CancellationTokenSource _cancellationTokenSource = new();
		readonly ILogger _logger;

		readonly UBAExecutor _owner;

		readonly BundleStorageClient _storage = BundleStorageClient.CreateInMemory(NullLogger.Instance);
		BlobLocator _ubaAgentLocator;

		readonly ServiceProvider _serviceProvider;
		readonly string _crypto;
		IComputeClient? _client;

		public struct Worker
		{
			public Task BackgroundTask { get; set; }
			public int NumLogicalCores { get; set; }
			public Stopwatch StartTime { get; set; }
			public bool Started { get; set; }
			public string Ip { get; set; }
			public ConnectionMetadataPort Port { get; set; }
			public ConnectionMetadataPort ProxyPort { get; set; }
		}

		readonly List<Worker> _workers = new();

		public UBAHordeSession(UBAExecutor owner, Uri hordeUri, AuthenticationHeaderValue? authHeader, string? pool, bool allowWine, int maxCores, bool strict, ConnectionMode? connectionMode, Encryption? encryption, ILogger logger)
		{
			_owner = owner;
			_hordeUri = hordeUri;
			_pool = pool;
			_allowWine = allowWine;
			_maxCores = maxCores;
			_strict = strict;
			_connectionMode = connectionMode;
			_encryption = encryption;
			_logger = logger;
			_crypto = owner.Crypto;

			void ConfigureHttpClient(HttpClient httpClient)
			{
				httpClient.BaseAddress = _hordeUri;
				httpClient.DefaultRequestHeaders.Authorization = authHeader;
			}

			ServiceCollection services = new();
			services.AddHttpClient<HordeHttpClient>(ConfigureHttpClient);
			_serviceProvider = services.BuildServiceProvider();

			if (connectionMode == ConnectionMode.Relay && String.IsNullOrEmpty(_crypto))
			{
				_crypto = UBAExecutor.CreateCrypto();
				_encryption = Encryption.Ssl;
			}
		}

		public async ValueTask DisposeAsync()
		{
			if (_workers.Count > 0) // Should handle double-dispose, prevent cancelling twice
			{
				_cancellationTokenSource.Cancel();

				for (int idx = _workers.Count - 1; idx >= 0; idx--)
				{
					await _workers[idx].BackgroundTask;
					_workers.RemoveAt(idx);
				}
			}

			if (_client != null)
			{
				// Set CPU resource need to zero (will also expire on the server if not updated)
				await UpdateCpuCoreNeedAsync(0);
				await _client.DisposeAsync();
				_client = null;
			}

			await _serviceProvider.DisposeAsync();
			_cancellationTokenSource.Dispose();

			_storage.Dispose();
		}

		public async Task InitAsync(bool useSentry, CancellationToken cancellationToken)
		{
			if (_client != null)
			{
				throw new InvalidOperationException("Session has already been initialized");
			}

			string? sessionId = null;
			string? jobId = Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			string? batchId = Environment.GetEnvironmentVariable("UE_HORDE_BATCHID");
			string? stepId = Environment.GetEnvironmentVariable("UE_HORDE_STEPID");

			if (jobId != null && batchId != null && stepId != null)
			{
				sessionId = $"{jobId}-{batchId}-{stepId}";
			}

			_client = new ServerComputeClient(_serviceProvider.GetRequiredService<IHttpClientFactory>(), sessionId, _logger);

			_logger.LogInformation("Creating tool bundle...");
			DirectoryReference ubaDir;
			List<string> agentFiles = new();
			if (OperatingSystem.IsWindows())
			{
#pragma warning disable CA1308 // Normalize strings to uppercase
				ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant());
#pragma warning restore CA1308 // Normalize strings to uppercase
				agentFiles.Add("UbaAgent.exe");
				if (useSentry)
				{
					agentFiles.Add("crashpad_handler.exe");
					agentFiles.Add("sentry.dll");
				}
			}
			else if (OperatingSystem.IsLinux())
			{
				agentFiles.Add("UbaAgent");
				bool bIsDebug = false;
				if (bIsDebug)
				{
					agentFiles.Add("UbaAgent.debug");
					agentFiles.Add("UbaAgent.sym");
					agentFiles.Add("libclang_rt.tsan.so"); // Needs to be copied from autosdk
					agentFiles.Add("llvm-symbolizer"); // Needs to be copied from autosdk
				}

				if (RuntimeInformation.ProcessArchitecture == Architecture.X64)
				{
					ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", "UnrealBuildAccelerator");
				}
				else if (RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
				{
					ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "LinuxArm64", "UnrealBuildAccelerator");
				}
				else
				{
					throw new PlatformNotSupportedException();
				}
			}
			else if (OperatingSystem.IsMacOS())
			{
				agentFiles.Add("UbaAgent");
				ubaDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Mac", "UnrealBuildAccelerator");
			}
			else
			{
				throw new PlatformNotSupportedException();
			}
			_ubaAgentLocator = await CreateToolAsync(ubaDir, agentFiles.Select(x => FileReference.Combine(ubaDir, x)), cancellationToken);
			_logger.LogInformation("Created tool bundle with locator {UbaAgentLocator}", _ubaAgentLocator.ToString());
		}

		async Task<BlobLocator> CreateToolAsync(DirectoryReference baseDir, IEnumerable<FileReference> files, CancellationToken cancellationToken)
		{
			BlobSerializerOptions serializerOptions = new();
			serializerOptions.Converters.Add(new InteriorChunkedDataNodeConverter(2)); // Lock to v2 for now. Could change based on protocol version.

			await using IBlobWriter writer = _storage.CreateBlobWriter(serializerOptions: serializerOptions);
			DirectoryNode sandbox = new();
			await sandbox.AddFilesAsync(baseDir, files, writer, cancellationToken: cancellationToken);
			IBlobRef<DirectoryNode> handle = await writer.WriteBlobAsync(sandbox, cancellationToken);
			await writer.FlushAsync(cancellationToken);
			return handle.GetLocator();
		}

		public int NumLogicalCores { get; private set; } = 0;

		public async void RemoveCompleteWorkersAsync()
		{
			for (int idx = 0; idx < _workers.Count; idx++)
			{
				Worker worker = _workers[idx];
				if (worker.BackgroundTask.IsCompleted)
				{
					await worker.BackgroundTask;
					NumLogicalCores -= worker.NumLogicalCores;
					_workers.RemoveAt(idx--);
				}
			}
		}

		public int QueuedUpCores()
		{
			int count = 0;
			foreach (Worker worker in _workers)
			{
				if (!worker.Started)
				{
					count += worker.NumLogicalCores;
				}
			}
			return count;
		}

		int _workerId = 0;

		static readonly HashSet<StringView> s_logProperties = new()
			{
				"ComputeIp",
				"CPU",
				"RAM",
				"DiskFreeSpace",
				"PhysicalCores",
				"LogicalCores"
			};

		public async Task<bool> AddWorkerAsync(Requirements requirements, UnrealBuildAcceleratorHordeConfig hordeConfig, CancellationToken cancellationToken)
		{
			if (_client == null)
			{
				throw new InvalidOperationException("Call init first");
			}

			PrefixLogger workerLogger = new($"[Worker{_workerId}]", _logger);

			const string UbaPortName = "UbaPort";
			const string UbaProxyPortName = "UbaProxyPort";
			const int UbaPort = 7001;
			const int UbaProxyPort = 7002;

			// Request ID that is unique per attempt to acquire the same compute lease/worker
			// Primarily for tracking worker demand on Horde server as UBAExecutor will repeatedly try adding a new worker
			string requestId = $"{_id}-worker-{_workerId}";
			IComputeLease? lease = null;
			try
			{
				Stopwatch stopwatch = Stopwatch.StartNew();
				ConnectionMetadataRequest cmr = new()
				{
					ModePreference = _connectionMode,
					Encryption = _encryption,
					Ports = { { UbaPortName, UbaPort }, { UbaProxyPortName, UbaProxyPort } }
				};
				lease = await _client.TryAssignWorkerAsync(_clusterId, requirements, requestId, cmr, workerLogger, cancellationToken);
				if (lease == null)
				{
					_logger.LogDebug("Unable to assign a remote worker");

					int missingNumCores = Math.Max(0, _maxCores - NumLogicalCores);
					await UpdateCpuCoreNeedAsync(missingNumCores, cancellationToken);
					return false;
				}

				_workerId++;

				workerLogger.LogDebug("Agent properties:");

				int numLogicalCores = 24; // Assume 24 if something goes wrong here and property is not found
				string computeIp = String.Empty;
				foreach (string property in lease.Properties)
				{
					int equalsIdx = property.IndexOf('=', StringComparison.OrdinalIgnoreCase);
					StringView propertyName = new(property, 0, equalsIdx);
					if (s_logProperties.Contains(propertyName))
					{
						_logger.LogDebug("  {Property}", property);

						if (propertyName == ResourceLogicalCores && Int32.TryParse(property.AsSpan(equalsIdx + 1), out int value))
						{
							numLogicalCores = value;
						}
						else if (propertyName == "ComputeIp")
						{
							computeIp = property[(equalsIdx + 1)..];
						}
					}
				}

				// When using relay connection mode, the IP will be relay server's IP
				string ip = String.IsNullOrEmpty(lease.Ip) ? computeIp : lease.Ip;

				if (!lease.Ports.TryGetValue(UbaPortName, out ConnectionMetadataPort? ubaPort))
				{
					ubaPort = new ConnectionMetadataPort(UbaPort, UbaPort);
				}

				if (!lease.Ports.TryGetValue(UbaProxyPortName, out ConnectionMetadataPort? ubaProxyPort))
				{
					ubaProxyPort = new ConnectionMetadataPort(UbaProxyPort, UbaProxyPort);
				}

				string exeName = OperatingSystem.IsWindows() ? "UbaAgent.exe" : "UbaAgent";
				BlobLocator locator = _ubaAgentLocator;
				Worker worker = new()
				{
					StartTime = stopwatch,
					NumLogicalCores = numLogicalCores,
					Ip = ip,
					Port = ubaPort,
					ProxyPort = ubaProxyPort,
				};
				worker.BackgroundTask = RunWorkerAsync(worker, lease, locator, exeName, workerLogger, hordeConfig, _cancellationTokenSource.Token);
				_workers.Add(worker);
				lease = null; // Will be disposed by RunWorkerAsync

				NumLogicalCores += numLogicalCores;
				return true;
			}
			finally
			{
				if (lease != null)
				{
					await lease.DisposeAsync();
				}
			}
		}

		async Task UpdateCpuCoreNeedAsync(int targetCoreCount, CancellationToken cancellationToken = default)
		{
			if (_client != null && _pool != null)
			{
				_logger.LogDebug("Setting CPU core need to {TargetCoreCount}", targetCoreCount);
				Dictionary<string, int> resourceNeeds = new() { { ResourceLogicalCores, targetCoreCount } };
				try
				{
					await _client.DeclareResourceNeedsAsync(_clusterId, _pool, resourceNeeds, cancellationToken);
				}
				catch (Exception e)
				{
					_logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, e, "Failed updating resource need to {TargetCoreCount} cores", targetCoreCount);
				}
			}
		}

		public static async Task<UBAHordeSession?> TryCreateHordeSessionAsync(UnrealBuildAcceleratorHordeConfig hordeConfig, UBAExecutor executor, bool bStrictErrors, ILogger logger, CancellationToken cancellationToken = default)
		{
			if (hordeConfig.bDisableHorde)
			{
				logger.LogInformation("Horde disabled via command line option.");
				return null;
			}

			string? server = hordeConfig.HordeServer;
			string? token = hordeConfig.HordeToken;
			string? oidcProvider = hordeConfig.HordeOidcProvider;

			if (String.IsNullOrEmpty(server))
			{
				server = Environment.GetEnvironmentVariable("UE_HORDE_URL");
				if (String.IsNullOrEmpty(server))
				{
					logger.LogInformation("Horde URL not specified in BuildConfiguration.xml or via UE_HORDE_URL environment variable");
					return null;
				}
				if (String.IsNullOrEmpty(token))
				{
					token = Environment.GetEnvironmentVariable("UE_HORDE_TOKEN");
				}
			}

			ConnectionMode? connectionMode = Enum.TryParse(hordeConfig.HordeConnectionMode, true, out ConnectionMode cm) ? cm : null;
			Encryption? encryption = Enum.TryParse(hordeConfig.HordeEncryption, true, out Encryption enc) ? enc : null;

			oidcProvider ??= Environment.GetEnvironmentVariable("UE_HORDE_OIDC_PROVIDER");

			bool hasOidcProvider = !String.IsNullOrEmpty(oidcProvider);
			logger.LogInformation("Horde URL: {Server}, Pool: {Pool}, Condition: {Condition}, OIDC: {OidcProvider}, Connection: {Connection} HordeEncryption: {Encryption}",
				server, hordeConfig.HordePool ?? "(none)", hordeConfig.HordeCondition ?? "(none)", hasOidcProvider ? oidcProvider! : "Disabled", connectionMode?.ToString() ?? "(none)", encryption?.ToString() ?? "(none)");
			try
			{
				if (String.IsNullOrEmpty(token) && hasOidcProvider)
				{
					token = await GetOidcBearerTokenAsync(null, oidcProvider!, logger, cancellationToken);
				}

				AuthenticationHeaderValue? authHeader = null;
				if (!String.IsNullOrEmpty(token))
				{
					authHeader = new AuthenticationHeaderValue("Bearer", token);
				}

				bool allowWine = hordeConfig.bHordeAllowWine && OperatingSystem.IsWindows();

				UBAHordeSession session = new(executor, new Uri(server), authHeader, hordeConfig.HordePool, allowWine, hordeConfig.HordeMaxCores, bStrictErrors, connectionMode, encryption, logger);
				await session.InitAsync(useSentry: !String.IsNullOrEmpty(hordeConfig.UBASentryUrl), cancellationToken);
				return session;
			}
			catch (TaskCanceledException)
			{
				return null;
			}
			catch (Exception ex)
			{
				logger.Log(bStrictErrors ? LogLevel.Error : LogLevel.Information, ex, "Unable to create Horde session: {Message}", ex.Message);
				return null;
			}
		}

		static async Task<string> GetOidcBearerTokenAsync(DirectoryReference? projectDir, string oidcProvider, ILogger logger, CancellationToken cancellationToken = default)
		{
			logger.LogInformation("Performing OIDC token refresh...");

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			IConfiguration providerConfiguration = ProviderConfigurationFactory.ReadConfiguration(Unreal.EngineDirectory.ToDirectoryInfo(), projectDir?.ToDirectoryInfo());
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(providerConfiguration, tokenStore, new List<string>() { oidcProvider });

			OidcTokenInfo result;
			try
			{
				result = await oidcTokenManager.GetAccessToken(oidcProvider, cancellationToken);
			}
			catch (NotLoggedInException)
			{
				result = await oidcTokenManager.Login(oidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {oidcProvider}");
			}

			logger.LogInformation("Received bearer token for {OidcProvider}", oidcProvider);
			return result.AccessToken;
		}

		async Task RunWorkerAsync(Worker self, IComputeLease lease, BlobLocator tool, string executable, ILogger logger, UnrealBuildAcceleratorHordeConfig hordeConfig, CancellationToken cancellationToken)
		{
			logger.LogDebug("Running worker task..");
			try
			{
				await using (_ = lease)
				{
					// Create a message channel on channel id 0. The Horde Agent always listens on this channel for requests.
					const int PrimaryChannelId = 0;
					using (AgentMessageChannel channel = lease.Socket.CreateAgentMessageChannel(PrimaryChannelId, 4 * 1024 * 1024))
					{
						logger.LogDebug("Waiting for attach...");

						TimeSpan attachTimeout = TimeSpan.FromSeconds(20.0);
						try
						{
							Task attachTask = channel.WaitForAttachAsync(cancellationToken).AsTask();
							await attachTask.WaitAsync(attachTimeout, cancellationToken);
						}
						catch (TimeoutException)
						{
							logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, "Waited {Time}s on attach message. Giving up", (int)attachTimeout.TotalSeconds);
							throw;
						}

						logger.LogDebug("Uploading files...");
						await channel.UploadFilesAsync("", tool, _storage.Backend, cancellationToken);

						string hordeHost = _owner.UBAConfig.Host;
						if (!String.IsNullOrEmpty(hordeConfig.HordeHost))
						{
							hordeHost = hordeConfig.HordeHost;
						}

						bool useListen = !String.IsNullOrEmpty(hordeConfig.HordeHost);
						List<string> arguments = new();

						if (useListen)
						{
							arguments.Add($"-Host={hordeHost}:{_owner.UBAConfig.Port}");
						}
						else
						{
							arguments.Add($"-Listen={self.Port.AgentPort}");
						}

						if (!String.IsNullOrEmpty(_crypto))
						{
							arguments.Add($"-crypto={_crypto}");
						}

						arguments.Add("-NoPoll");
						arguments.Add("-Quiet");
						if (!String.IsNullOrEmpty(hordeConfig.UBASentryUrl))
						{
							arguments.Add($"-Sentry=\"{hordeConfig.UBASentryUrl}\"");
						}
						arguments.Add("-ProxyPort=" + self.ProxyPort.AgentPort);
						if (_owner.UBAConfig.bUseQuic)
						{
							arguments.Add("-quic");
						}
						//arguments.Add("-NoStore");
						//arguments.Add("-KillRandom"); // For debugging

						if (System.OperatingSystem.IsMacOS())
						{
							// we need to populate the cas with all known xcodes so we can serve all the ones we have installed
							string xcodeVersion = Utils.RunLocalProcessAndReturnStdOut("/bin/sh", "-c '/usr/bin/defaults read $(xcode-select -p)/../version.plist ProductBuildVersion");
							arguments.Add($"-populateCasFromXcodeVersion={xcodeVersion}");
						}

						arguments.Add("-Dir=%UE_HORDE_SHARED_DIR%\\Uba");
						arguments.Add("-Eventfile=%UE_HORDE_TERMINATION_SIGNAL_FILE%");
						arguments.Add("-MaxIdle=15");
						if (_owner.UBAConfig.bLogEnabled)
						{
							arguments.Add("-Log");
						}

						LogLevel logLevel = _owner.UBAConfig.bDetailedLog ? LogLevel.Information : LogLevel.Debug;

						logger.Log(logLevel, "Executing child process: {Executable} {Arguments}", executable, CommandLineArguments.Join(arguments));

						ExecuteProcessFlags execFlags = _allowWine ? ExecuteProcessFlags.UseWine : ExecuteProcessFlags.None;
						await using AgentManagedProcess process = await channel.ExecuteAsync(executable, arguments, null, null, execFlags, cancellationToken);
						bool shouldConnect = !useListen;
						self.Started = true;
						string? line;

						while ((line = await process.ReadLineAsync(cancellationToken)) != null)
						{
							logger.Log(logLevel, "{Line}", line);

							if (shouldConnect && line.Contains("Listening on", StringComparison.OrdinalIgnoreCase)) // This log entry means that the agent is ready for connections.
							{
								long totalMs = self.StartTime.ElapsedMilliseconds;
								logger.LogInformation("Connecting to UbaAgent on {Ip}:{Port} (local agent port {AgentPort}) {Seconds}.{Milliseconds} seconds after assigned",
									self.Ip, self.Port.Port, self.Port.AgentPort, totalMs / 1000, totalMs % 1000);

								_owner.Server!.AddClient(self.Ip, self.Port.Port, _crypto);
								shouldConnect = false;
							}
						}
						logger.LogDebug("Shutting down process");
					}

					logger.LogDebug("Closing channel");
					await lease.CloseAsync(cancellationToken);
				}
			}
			catch (TimeoutException)
			{
			}
			catch (Exception ex)
			{
				if (!cancellationToken.IsCancellationRequested)
				{
					logger.Log(_strict ? LogLevel.Error : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, ex, "Exception in worker task: {Ex}", ex.ToString());

					// Add additional properties to aid debugging
					logger.LogInformation(KnownLogEvents.Systemic_Horde_Compute, ex, "UBA agent locator {UBAAgentLocator}", _ubaAgentLocator.ToString());
				}
			}
		}
	}

	class UBAAgentCoordinatorHorde : IUBAAgentCoordinator, IDisposable
	{
		public UBAAgentCoordinatorHorde(ILogger logger, UnrealBuildAcceleratorConfig ubaConfig, CommandLineArguments? additionalArguments = null)
		{
			_logger = logger;
			_ubaConfig = ubaConfig;

			XmlConfig.ApplyTo(HordeConfig);
			additionalArguments?.ApplyTo(HordeConfig);

			// Sentry is currently unsupported for non-Windows and non-x64
			if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
			{
				HordeConfig.UBASentryUrl = null;
			}
		}

		public DirectoryReference? GetUBARootDir()
		{
			DirectoryReference? hordeSharedDir = DirectoryReference.FromString(Environment.GetEnvironmentVariable("UE_HORDE_SHARED_DIR"));
			if (hordeSharedDir != null)
			{
				return DirectoryReference.Combine(hordeSharedDir, "UbaHost");
			}
			return null;
		}

		public async Task InitAsync(UBAExecutor executor)
		{
			if (_ubaConfig.bDisableRemote)
			{
				return;
			}

			_cancellationSource = new CancellationTokenSource();
			_hordeSessionTask = UBAHordeSession.TryCreateHordeSessionAsync(HordeConfig, executor, _ubaConfig.bStrict, _logger, _cancellationSource.Token);
			await _hordeSessionTask;
		}

		public void Start(ImmediateActionQueue queue, Func<LinkedAction, bool> canRunRemotely)
		{
			int timerPeriod = 5000;
			bool shownNoAgentsFoundMessage = false;

			if (_hordeSessionTask == null)
			{
				return;
			}
			_timer = new(async (_) =>
			{
				_timer?.Change(Timeout.Infinite, Timeout.Infinite);

				if (_cancellationSource!.IsCancellationRequested)
				{
					return;
				}

				UBAHordeSession? hordeSession = await _hordeSessionTask!;

				if (hordeSession == null)
				{
					return;
				}

				hordeSession.RemoveCompleteWorkersAsync();

				if (queue.IsDone)
				{
					return;
				}

				// We are assuming all active logical cores are already being used.. so queueWeight is essentially work that could be executed but can't because of bandwidth
				double queueThreshold = _ubaConfig.bForceBuildAllRemote ? 0 : 5;

				try
				{
					double queueWeight = queue.EnumerateReadyToCompileActions().Where(x => canRunRemotely(x)).Sum(x => x.Weight);

					queueWeight -= hordeSession.QueuedUpCores();
					while (true)
					{
						int currentLogicalCores = hordeSession.NumLogicalCores;

						if (queueWeight <= queueThreshold || currentLogicalCores >= HordeConfig.HordeMaxCores || _cancellationSource!.IsCancellationRequested)
						{
							break;
						}

						Requirements requirements = new()
						{
							Exclusive = true
						};

						if (!String.IsNullOrEmpty(HordeConfig.HordePool))
						{
							requirements.Pool = HordeConfig.HordePool;
						}

						if (HordeConfig.HordeCondition != null)
						{
							requirements.Condition = Condition.Parse(HordeConfig.HordeCondition);
						}

						if (!await hordeSession.AddWorkerAsync(requirements, HordeConfig, _cancellationSource.Token))
						{
							_logger.LogDebug("No additional workers available");
							break;
						}
						int coresAdded = hordeSession.NumLogicalCores - currentLogicalCores;
						queueWeight -= coresAdded;
					}
				}
				catch (NoComputeAgentsFoundException ex)
				{
					if (!shownNoAgentsFoundMessage)
					{
						_logger.Log(_ubaConfig.bStrict ? LogLevel.Warning : LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, ex, "No agents found matching requirements (cluster: {ClusterId}, requirements: {Requirements})", ex.ClusterId, ex.Requirements);
						shownNoAgentsFoundMessage = true;
					}
				}
				catch (Exception ex)
				{
					if (!_cancellationSource!.IsCancellationRequested)
					{
						if (_ubaConfig.bStrict)
						{
							_logger.Log(LogLevel.Error, KnownLogEvents.Systemic_Horde_Compute, ex, "Unable to get worker: {Ex}", ex.ToString());
						}
						else
						{
							_logger.Log(LogLevel.Information, KnownLogEvents.Systemic_Horde_Compute, "Unable to get worker: {Ex}", ex.ToString());
						}
					}
				}

				_timer?.Change(timerPeriod, Timeout.Infinite);
			}, null, HordeConfig.HordeDelay * 1000, timerPeriod);
		}

		public void Stop()
		{
			_cancellationSource?.Cancel();
		}

		public async Task CloseAsync()
		{
			_cancellationSource?.Cancel();

			if (_hordeSessionTask == null)
			{
				return;
			}

			UBAHordeSession? hordeSession = await _hordeSessionTask;
			if (hordeSession != null)
			{
				await hordeSession.DisposeAsync();
			}
		}

		public void Dispose()
		{
			Stop();
			CloseAsync().Wait();
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				_cancellationSource?.Dispose();
				_cancellationSource = null;
				_timer?.Dispose();
				_timer = null;
			}
		}

		readonly ILogger _logger;
		readonly UnrealBuildAcceleratorConfig _ubaConfig;
		UnrealBuildAcceleratorHordeConfig HordeConfig { get; init; } = new();

		CancellationTokenSource? _cancellationSource;
		Task<UBAHordeSession?>? _hordeSessionTask;
		Timer? _timer;
	}
}
