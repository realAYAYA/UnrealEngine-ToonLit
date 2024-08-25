// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Sessions;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Utility;
using Horde.Common.Rpc;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Interface for about the current session. 
	/// </summary>
	interface ISession : IAsyncDisposable
	{
		/// <summary>
		/// URL of the server
		/// </summary>
		Uri ServerUrl { get; }

		/// <summary>
		/// The agent identifier
		/// </summary>
		AgentId AgentId { get; }

		/// <summary>
		/// Identifier for the current session
		/// </summary>
		SessionId SessionId { get; }

		/// <summary>
		/// Token to use for connection to the server
		/// </summary>
		string Token { get; }

		/// <summary>
		/// Connection to the server
		/// </summary>
		IRpcConnection RpcConnection { get; }

		/// <summary>
		/// A gRPC channel authenticated for this session
		/// </summary>
		GrpcChannel GrpcChannel { get; }

		/// <summary>
		/// Working directory for sandboxes etc..
		/// </summary>
		DirectoryReference WorkingDir { get; }

		/// <summary>
		/// Terminate all processes in running in the working directory
		/// </summary>
		/// <param name="condition">Flags indicating which processes to terminate</param>
		/// <param name="logger">Logger for any diagnostic messages</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task TerminateProcessesAsync(TerminateCondition condition, ILogger logger, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Interface for a factory to create sessions
	/// </summary>
	interface ISessionFactory
	{
		/// <summary>
		/// Creates a new session
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New session information</returns>
		public Task<ISession> CreateAsync(CancellationToken cancellationToken);
	}

	/// <summary>
	/// Information about the current session. 
	/// </summary>
	sealed class Session : ISession
	{
		/// <inheritdoc/>
		public Uri ServerUrl { get; }

		/// <inheritdoc/>
		public AgentId AgentId { get; }

		/// <inheritdoc/>
		public SessionId SessionId { get; }

		/// <inheritdoc/>
		public string Token { get; }

		/// <inheritdoc/>
		public IRpcConnection RpcConnection { get; }

		/// <inheritdoc/>
		public GrpcChannel GrpcChannel { get; }

		/// <summary>
		/// Working directory for sandboxes etc..
		/// </summary>
		public DirectoryReference WorkingDir { get; }

		readonly IReadOnlyDictionary<string, TerminateCondition> _processNamesToTerminate;

		/// <summary>
		/// Constructor
		/// </summary>
		public Session(Uri serverUrl, AgentId agentId, SessionId sessionId, string token, IRpcConnection rpcConnection, GrpcChannel grpcChannel, DirectoryReference workingDir, IReadOnlyDictionary<string, TerminateCondition> processNamesToTerminate)
		{
			ServerUrl = serverUrl;
			AgentId = agentId;
			SessionId = sessionId;
			Token = token;
			RpcConnection = rpcConnection;
			GrpcChannel = grpcChannel;
			WorkingDir = workingDir;

			_processNamesToTerminate = processNamesToTerminate;
		}

		public class AgentRegistrationList
		{
			public List<AgentRegistration> Entries { get; set; } = new List<AgentRegistration>();
		}

		public record class AgentRegistration(Uri Server, string Id, string Token);

		/// <summary>
		/// Creates a new agent session
		/// </summary>
		public static async Task<Session> CreateAsync(CapabilitiesService capabilitiesService, GrpcService grpcService, StatusService statusService, IOptions<AgentSettings> settings, ILogger logger, CancellationToken cancellationToken)
		{
			AgentSettings currentSettings = settings.Value;

			// Get the working directory
			if (currentSettings.WorkingDir == null)
			{
				throw new Exception("WorkingDir is not set. Unable to run service.");
			}

			DirectoryReference workingDir = currentSettings.WorkingDir;
			logger.LogInformation("WorkingDir: {WorkingDir}", workingDir);
			DirectoryReference.CreateDirectory(workingDir);

			// Print the server info
			ServerProfile serverProfile = currentSettings.GetCurrentServerProfile();
			logger.LogInformation("Server: {Server}", serverProfile.Url);

			// Show the worker capabilities
			AgentCapabilities capabilities = await capabilitiesService.GetCapabilitiesAsync(workingDir);
			if (capabilities.Properties.Count > 0)
			{
				logger.LogInformation("Global:");
				foreach (string property in capabilities.Properties)
				{
					logger.LogInformation("  {AgentProperty}", property);
				}
			}
			foreach (DeviceCapabilities device in capabilities.Devices)
			{
				logger.LogInformation("{DeviceName} Device:", device.Handle);
				foreach (string property in device.Properties)
				{
					logger.LogInformation("   {DeviceProperty}", property);
				}
			}

			// Mount all the necessary network shares. Currently only supported on Windows.
			if (currentSettings.ShareMountingEnabled && RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				foreach (MountNetworkShare share in currentSettings.Shares)
				{
					if (share.MountPoint != null && share.RemotePath != null)
					{
						logger.LogInformation("Mounting {RemotePath} as {MountPoint}", share.RemotePath, share.MountPoint);
						NetworkShare.Mount(share.MountPoint, share.RemotePath);
					}
				}
			}

			// Get the location of the registration file
			FileReference registrationFile = GetRegistrationFile();

			// Read existing settings if possible
			AgentRegistrationList registrationList = await ReadRegistrationListAsync(registrationFile, cancellationToken);

			// If they aren't valid, create a new agent registration
			AgentRegistration? registrationInfo = registrationList.Entries.FirstOrDefault(x => x.Server == grpcService.ServerProfile.Url);
			if (registrationInfo == null)
			{
				statusService.Set(AgentStatusMessage.WaitingForEnrollment);

				registrationInfo = await RegisterAgentAsync(grpcService, currentSettings, capabilities, logger, cancellationToken);
				registrationList.Entries.Add(registrationInfo);

				await WriteRegistrationListAsync(registrationFile, registrationList, cancellationToken);
				logger.LogInformation("Created agent (Id={AgentId}). Settings saved to {File}.", registrationInfo.Id, registrationFile);
			}

			// Create the session
			statusService.Set(AgentStatusMessage.ConnectingToServer);

			CreateSessionResponse createSessionResponse;
			using (GrpcChannel channel = await grpcService.CreateGrpcChannelAsync(registrationInfo.Token, cancellationToken))
			{
				HordeRpc.HordeRpcClient rpcClient = new HordeRpc.HordeRpcClient(channel);

				// Create the session information
				CreateSessionRequest sessionRequest = new CreateSessionRequest();
				sessionRequest.Id = registrationInfo.Id;
				sessionRequest.Status = RpcAgentStatus.Ok;
				sessionRequest.Capabilities = capabilities;
				sessionRequest.Version = AgentApp.Version;

				// Create a session
				try
				{
					createSessionResponse = await rpcClient.CreateSessionAsync(sessionRequest, null, null, cancellationToken);
					logger.LogInformation("Created session. AgentName={AgentName} SessionId={SessionId}", currentSettings.GetAgentName(), createSessionResponse.SessionId);
				}
				catch (RpcException ex) when (ex.StatusCode == StatusCode.PermissionDenied)
				{
					Uri serverUrl = grpcService.ServerProfile.Url;
					if (registrationList.Entries.RemoveAll(x => x.Server == serverUrl) > 0)
					{
						logger.LogError(ex, "Unable to create session. Invalidating agent registration for server {ServerUrl}.", serverUrl);
						await WriteRegistrationListAsync(registrationFile, registrationList, cancellationToken);
					}
					throw;
				}
			}

			Func<CancellationToken, Task<GrpcChannel>> createGrpcChannelAsync = ctx => grpcService.CreateGrpcChannelAsync(createSessionResponse.Token, ctx);

			// Open a connection to the server
#pragma warning disable CA2000 // False positive; ownership is transferred to new Session object.
			IRpcConnection rpcConnection = new RpcConnection(createGrpcChannelAsync, logger);
			GrpcChannel sessionGrpcChannel = await grpcService.CreateGrpcChannelAsync(createSessionResponse.Token, cancellationToken);
			return new Session(serverProfile.Url, new AgentId(createSessionResponse.AgentId), SessionId.Parse(createSessionResponse.SessionId), createSessionResponse.Token, rpcConnection, sessionGrpcChannel, workingDir, currentSettings.GetProcessesToTerminateMap());
#pragma warning restore CA2000
		}

		/// <summary>
		/// Dispose of the current session
		/// </summary>
		/// <returns></returns>
		public async ValueTask DisposeAsync()
		{
			await RpcConnection.DisposeAsync();
		}

		static FileReference GetRegistrationFile()
		{
			// Get the location of the registration file
			DirectoryReference? settingsDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
			if (settingsDir == null)
			{
				settingsDir = DirectoryReference.GetCurrentDirectory();
			}
			else
			{
				settingsDir = DirectoryReference.Combine(settingsDir, "Epic Games", "Horde", "Agent");
			}

			FileReference settingsFile = FileReference.Combine(settingsDir, "servers.json");
			return settingsFile;
		}

		static async Task<AgentRegistrationList> ReadRegistrationListAsync(FileReference settingsFile, CancellationToken cancellationToken)
		{
			AgentRegistrationList? registrationList = null;
			if (FileReference.Exists(settingsFile))
			{
				byte[] settingsData = await FileReference.ReadAllBytesAsync(settingsFile, cancellationToken);
				registrationList = JsonSerializer.Deserialize<AgentRegistrationList>(settingsData, AgentApp.DefaultJsonSerializerOptions);
				registrationList?.Entries.RemoveAll(x => x.Server == null || x.Id == null || x.Token == null);
			}
			return registrationList ??= new AgentRegistrationList();
		}

		static async Task WriteRegistrationListAsync(FileReference settingsFile, AgentRegistrationList registrationList, CancellationToken cancellationToken)
		{
			byte[] data = JsonSerializer.SerializeToUtf8Bytes(registrationList, new JsonSerializerOptions(AgentApp.DefaultJsonSerializerOptions) { WriteIndented = true });
			DirectoryReference.CreateDirectory(settingsFile.Directory);
			await FileReference.WriteAllBytesAsync(settingsFile, data, cancellationToken);
		}

		static async Task<AgentRegistration> RegisterAgentAsync(GrpcService grpcService, AgentSettings agentSettings, AgentCapabilities capabilities, ILogger logger, CancellationToken cancellationToken)
		{
			ServerProfile serverProfile = agentSettings.GetCurrentServerProfile();
			using GrpcChannel grpcChannel = await grpcService.CreateGrpcChannelAsync(serverProfile.Token, cancellationToken);

			if (!String.IsNullOrEmpty(serverProfile.Token))
			{
				logger.LogInformation("Registering agent directly...");
				HordeRpc.HordeRpcClient rpcClient = new HordeRpc.HordeRpcClient(grpcChannel);

				CreateAgentRequest createAgentRequest = new CreateAgentRequest();
				createAgentRequest.Name = agentSettings.GetAgentName();
				createAgentRequest.Ephemeral = agentSettings.Ephemeral;

				CreateAgentResponse createAgentResponse = await rpcClient.CreateAgentAsync(createAgentRequest, null, null, cancellationToken);
				return new AgentRegistration(serverProfile.Url, createAgentResponse.Id, createAgentResponse.Token);
			}

			const string FormatString = "$(CPU) ($(LogicalCores) cores, $(RAM)gb RAM, $(OSDistribution))";
			string description = StringUtils.ExpandProperties(FormatString, name => GetProperty(capabilities, name));

			string registrationKey = StringUtils.FormatHexString(RandomNumberGenerator.GetBytes(64));
			for (; ; )
			{
				logger.LogInformation("Waiting for agent to be approved...");
				EnrollmentRpc.EnrollmentRpcClient rpcClient = new EnrollmentRpc.EnrollmentRpcClient(grpcChannel);

				EnrollAgentRequest enrollAgentRequest = new EnrollAgentRequest();
				enrollAgentRequest.Key = registrationKey;
				enrollAgentRequest.HostName = Environment.MachineName;
				enrollAgentRequest.Description = description;

				try
				{
					using AsyncDuplexStreamingCall<EnrollAgentRequest, EnrollAgentResponse> call = rpcClient.EnrollAgent(cancellationToken: cancellationToken);
					await call.RequestStream.WriteAsync(enrollAgentRequest, cancellationToken);

					Task delayTask = Task.Delay(TimeSpan.FromSeconds(10), cancellationToken);
					Task<bool> responseTask = call.ResponseStream.MoveNext(cancellationToken);
					Task completeTask = await Task.WhenAny(delayTask, responseTask);

					if (completeTask == delayTask)
					{
						await call.RequestStream.WriteAsync(enrollAgentRequest, cancellationToken);
					}

					if (await responseTask)
					{
						EnrollAgentResponse enrollAgentResponse = call.ResponseStream.Current;
						return new AgentRegistration(serverProfile.Url, enrollAgentResponse.Id, enrollAgentResponse.Token);
					}
				}
				catch (RpcException ex)
				{
					logger.LogWarning(ex, "Exception in RPC: {Message}", ex.Message);
				}
			}
		}

		static string? GetProperty(AgentCapabilities capabilities, string name)
		{
			foreach (string property in capabilities.Devices[0].Properties)
			{
				if (property.Length > name.Length && property[name.Length] == '=' && property.StartsWith(name, StringComparison.OrdinalIgnoreCase))
				{
					return property.Substring(name.Length + 1);
				}
			}
			return null;
		}

		/// <inheritdoc/>
		public Task TerminateProcessesAsync(TerminateCondition condition, ILogger logger, CancellationToken cancellationToken)
		{
			// Terminate child processes from any previous runs
			ProcessUtils.TerminateProcesses(x => ShouldTerminateProcess(x, condition), logger, cancellationToken);
			return Task.CompletedTask;
		}

		/// <summary>
		/// Callback for determining whether a process should be terminated
		/// </summary>
		bool ShouldTerminateProcess(FileReference imageFile, TerminateCondition condition)
		{
			if (imageFile.IsUnderDirectory(WorkingDir))
			{
				return true;
			}

			string fileName = imageFile.GetFileName();
			if (_processNamesToTerminate.TryGetValue(fileName, out TerminateCondition terminateFlags))
			{
				if (terminateFlags == TerminateCondition.None || (terminateFlags & condition) != 0)
				{
					return true;
				}
			}

			return false;
		}
	}

	/// <summary>
	/// Creates session objects
	/// </summary>
	class SessionFactory : ISessionFactory
	{
		readonly CapabilitiesService _capabilitiesService;
		readonly GrpcService _grpcService;
		readonly StatusService _statusService;
		readonly IOptions<AgentSettings> _settings;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public SessionFactory(CapabilitiesService capabilitiesService, GrpcService grpcService, StatusService statusService, IOptions<AgentSettings> settings, ILogger<SessionFactory> logger)
		{
			_capabilitiesService = capabilitiesService;
			_grpcService = grpcService;
			_statusService = statusService;
			_settings = settings;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<ISession> CreateAsync(CancellationToken cancellationToken) => await Session.CreateAsync(_capabilitiesService, _grpcService, _statusService, _settings, _logger, cancellationToken);
	}
}
