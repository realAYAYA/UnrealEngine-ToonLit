// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Sockets;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using EpicGames.Horde.Logs;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Leases.Handlers
{
	/// <summary>
	/// Handler for compute tasks
	/// </summary>
	class ComputeHandler : LeaseHandler<ComputeTask>
	{
		readonly ComputeListenerService _listenerService;
		readonly IServerLoggerFactory _serverLoggerFactory;
		readonly AgentSettings _settings;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeHandler(ComputeListenerService listenerService, IServerLoggerFactory serverLoggerFactory, IOptions<AgentSettings> settings)
		{
			_listenerService = listenerService;
			_serverLoggerFactory = serverLoggerFactory;
			_settings = settings.Value;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ComputeTask computeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			await using IServerLogger? serverLogger = (computeTask.LogId != null) ? _serverLoggerFactory.CreateLogger(session, LogId.Parse(computeTask.LogId), localLogger, null, LogLevel.Trace) : null;
			ILogger logger = serverLogger ?? localLogger;

			if (!String.IsNullOrEmpty(computeTask.ParentLeaseId))
			{
				logger.LogInformation("Parent lease: {LeaseId}", computeTask.ParentLeaseId);
			}

			logger.LogInformation("Starting compute task (lease {LeaseId}). Waiting for connection with nonce {Nonce}...", leaseId, StringUtils.FormatHexString(computeTask.Nonce.Span));
			ClearTerminationSignalFile(logger);

			TcpClient? tcpClient = null;
			try
			{
				const int TimeoutSeconds = 30;

				tcpClient = await _listenerService.WaitForClientAsync(new ByteString(computeTask.Nonce.Memory), TimeSpan.FromSeconds(TimeoutSeconds), cancellationToken);
				if (tcpClient == null)
				{
					logger.LogInformation("Timed out waiting for connection after {Time}s", TimeoutSeconds);
					return LeaseResult.Success;
				}

				logger.LogInformation("Matched connection for {Nonce}", StringUtils.FormatHexString(computeTask.Nonce.Span));

				using (CancellationTokenSource cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
				{
					await using ComputeTransport innerTransport = await CreateTransportAsync(computeTask, tcpClient.Client, cts.Token);
					await using IdleTimeoutTransport idleTimeoutTransport = new(innerTransport);

					await using BackgroundTask timeoutTask = BackgroundTask.StartNew(ctx => idleTimeoutTransport.StartWatchdogTimerAsync(cts, logger, ctx));
					try
					{
						ComputeProtocol protocol = (ComputeProtocol)computeTask.Protocol;
						logger.LogInformation("Using compute protocol version {Version}", (int)protocol);

						await using (RemoteComputeSocket socket = new RemoteComputeSocket(idleTimeoutTransport, protocol, logger))
						{
							DirectoryReference sandboxDir = DirectoryReference.Combine(session.WorkingDir, "Sandbox", leaseId.ToString());
							try
							{
								DirectoryReference.CreateDirectory(sandboxDir);

								DirectoryReference sharedDir = DirectoryReference.Combine(session.WorkingDir, "Saved");
								DirectoryReference.CreateDirectory(sharedDir);

								Dictionary<string, string?> newEnvVars = new Dictionary<string, string?>();
								newEnvVars["UE_HORDE_SHARED_DIR"] = sharedDir.FullName;
								newEnvVars["UE_HORDE_TERMINATION_SIGNAL_FILE"] = _settings.GetTerminationSignalFile().FullName;

								AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, newEnvVars, false, _settings.WineExecutablePath, _settings.ContainerEngineExecutablePath, logger);
								await worker.RunAsync(socket, cts.Token);
								await socket.CloseAsync(cts.Token);
								return LeaseResult.Success;
							}
							finally
							{
								FileUtils.ForceDeleteDirectory(sandboxDir);
							}
						}
					}
					catch (OperationCanceledException ex) when (cts.IsCancellationRequested && idleTimeoutTransport.TimeSinceActivity > idleTimeoutTransport.NoDataTimeout)
					{
						logger.LogError(ex, "Lease was terminated due to no data being received for {Time} seconds", (int)idleTimeoutTransport.NoDataTimeout.TotalSeconds);
						return LeaseResult.Failed;
					}
				}
			}
			catch (OperationCanceledException)
			{
				throw;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Exception while executing compute task: {Message}", ex.Message);
				return LeaseResult.Failed;
			}
			finally
			{
				tcpClient?.Dispose();
			}
		}

		private static async Task<ComputeTransport> CreateTransportAsync(ComputeTask computeTask, Socket socket, CancellationToken cancellationToken)
		{
			switch (computeTask.Encryption)
			{
				case ComputeEncryption.SslRsa2048:
				case ComputeEncryption.SslEcdsaP256:
					TcpSslTransport sslTransport = new(socket, computeTask.Certificate.ToByteArray(), true);
					await sslTransport.AuthenticateAsync(cancellationToken);
					return sslTransport;

				case ComputeEncryption.Aes:
#pragma warning disable CA2000 // Dispose objects before losing scope
					return new AesTransport(new TcpTransport(socket), computeTask.Key.ToByteArray(), computeTask.Nonce.ToByteArray());
#pragma warning restore CA2000 // Restore CA2000

				case ComputeEncryption.Unspecified:
				case ComputeEncryption.None:
				default:
					return new TcpTransport(socket);
			}
		}

		private void ClearTerminationSignalFile(ILogger logger)
		{
			string path = _settings.GetTerminationSignalFile().FullName;
			try
			{
				File.Delete(path);
			}
			catch (Exception e)
			{
				// If this file is not removed and lingers on from previous executions,
				// new compute tasks may pick it up and erroneously decide to terminate.
				logger.LogError(e, "Unable to delete termination signal file {Path}", path);
			}
		}
	}
}

