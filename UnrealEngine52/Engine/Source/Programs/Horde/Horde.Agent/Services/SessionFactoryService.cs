// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Net.Client;
using Horde.Agent.Utility;
using HordeCommon;
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
		string AgentId { get; }

		/// <summary>
		/// Identifier for the current session
		/// </summary>
		string SessionId { get; }

		/// <summary>
		/// Token to use for connection to the server
		/// </summary>
		string Token { get; }

		/// <summary>
		/// Connection to the server
		/// </summary>
		IRpcConnection RpcConnection { get; }

		/// <summary>
		/// Working directory for sandboxes etc..
		/// </summary>
		DirectoryReference WorkingDir { get; }

		/// <summary>
		/// Terminate all processes in running in the working directory
		/// </summary>
		/// <param name="logger">Logger for any diagnostic messages</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task TerminateProcessesAsync(ILogger logger, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Interface for a factory to create sessions
	/// </summary>
	interface ISessionFactoryService
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
		/// <summary>
		/// URL of the server
		/// </summary>
		public Uri ServerUrl { get; }

		/// <summary>
		/// The agent identifier
		/// </summary>
		public string AgentId { get; }

		/// <summary>
		/// Identifier for the current session
		/// </summary>
		public string SessionId { get; }

		/// <summary>
		/// Token to use for connection to the server
		/// </summary>
		public string Token { get; }

		/// <summary>
		/// Connection to the server
		/// </summary>
		public IRpcConnection RpcConnection { get; }

		/// <summary>
		/// Working directory for sandboxes etc..
		/// </summary>
		public DirectoryReference WorkingDir { get; }

		readonly HashSet<string> _processNamesToTerminate;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public Session(Uri serverUrl, string agentId, string sessionId, string token, IRpcConnection rpcConnection, DirectoryReference workingDir, IEnumerable<string> processNamesToTerminate, ILogger logger)
		{
			ServerUrl = serverUrl;
			AgentId = agentId;
			SessionId = sessionId;
			Token = token;
			RpcConnection = rpcConnection;
			WorkingDir = workingDir;

			_processNamesToTerminate = new HashSet<string>(processNamesToTerminate, StringComparer.OrdinalIgnoreCase);
			_logger = logger;
		}

		/// <summary>
		/// Creates a new agent session
		/// </summary>
		/// <param name="capabilitiesService"></param>
		/// <param name="grpcService"></param>
		/// <param name="settings"></param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken">Indicates that the service is trying to stop</param>
		/// <returns>Async task</returns>
		public static async Task<Session> CreateAsync(CapabilitiesService capabilitiesService, GrpcService grpcService, IOptions<AgentSettings> settings, ILogger logger, CancellationToken cancellationToken)
		{
			AgentSettings currentSettings = settings.Value;

			// Get the working directory
			if (currentSettings.WorkingDir == null)
			{
				throw new Exception("WorkingDir is not set. Unable to run service.");
			}

			DirectoryReference baseDir = new FileReference(Assembly.GetExecutingAssembly().Location).Directory;
			DirectoryReference workingDir = DirectoryReference.Combine(baseDir, currentSettings.WorkingDir);
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

			// Create the session
			CreateSessionResponse createSessionResponse;
			using (GrpcChannel channel = grpcService.CreateGrpcChannel(serverProfile.Token))
			{
				HordeRpc.HordeRpcClient rpcClient = new HordeRpc.HordeRpcClient(channel);

				// Create the session information
				CreateSessionRequest sessionRequest = new CreateSessionRequest();
				sessionRequest.Name = currentSettings.GetAgentName();
				sessionRequest.Status = AgentStatus.Ok;
				sessionRequest.Capabilities = capabilities;
				sessionRequest.Version = Program.Version;

				// Create a session
				createSessionResponse = await rpcClient.CreateSessionAsync(sessionRequest, null, null, cancellationToken);
				logger.LogInformation("Created session. AgentName={AgentName} SessionId={SessionId}", currentSettings.GetAgentName(), createSessionResponse.SessionId);
			}

			Func<GrpcChannel> createGrpcChannel = () => grpcService.CreateGrpcChannel(createSessionResponse.Token);

			// Open a connection to the server
#pragma warning disable CA2000 // False positive; ownership is transferred to new Session object.
			IRpcConnection rpcConnection = new RpcConnection(createGrpcChannel, logger);
			return new Session(serverProfile.Url, createSessionResponse.AgentId, createSessionResponse.SessionId, createSessionResponse.Token, rpcConnection, workingDir, currentSettings.ProcessNamesToTerminate, logger);
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

		/// <inheritdoc/>
		public Task TerminateProcessesAsync(ILogger logger, CancellationToken cancellationToken)
		{
			// Terminate child processes from any previous runs
			ProcessUtils.TerminateProcesses(ShouldTerminateProcess, logger, cancellationToken);
			return Task.CompletedTask;
		}

		/// <summary>
		/// Callback for determining whether a process should be terminated
		/// </summary>
		bool ShouldTerminateProcess(FileReference imageFile)
		{
			if (imageFile.IsUnderDirectory(WorkingDir))
			{
				return true;
			}

			string fileName = imageFile.GetFileName();
			if (_processNamesToTerminate.Contains(fileName))
			{
				return true;
			}

			return false;
		}
	}

	/// <summary>
	/// Creates session objects
	/// </summary>
	class SessionFactoryService : ISessionFactoryService
	{
		readonly CapabilitiesService _capabilitiesService;
		readonly GrpcService _grpcService;
		readonly IOptions<AgentSettings> _settings;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public SessionFactoryService(CapabilitiesService capabilitiesService, GrpcService grpcService, IOptions<AgentSettings> settings, ILogger<SessionFactoryService> logger)
		{
			_capabilitiesService = capabilitiesService;
			_grpcService = grpcService;
			_settings = settings;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<ISession> CreateAsync(CancellationToken cancellationToken) => await Session.CreateAsync(_capabilitiesService, _grpcService, _settings, _logger, cancellationToken);
	}
}
