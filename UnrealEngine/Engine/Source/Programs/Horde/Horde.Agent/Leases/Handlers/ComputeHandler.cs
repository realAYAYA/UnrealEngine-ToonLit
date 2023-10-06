// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Diagnostics;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	/// <summary>
	/// Handler for compute tasks
	/// </summary>
	class ComputeHandler : LeaseHandler<ComputeTask>
	{
		class TcpTransportWithTimeout : IComputeTransport
		{
			readonly TcpTransport _inner;
			long _lastPingTicks;

			public long Position => _inner.Position;

			public TcpTransportWithTimeout(Socket socket)
			{
				_inner = new TcpTransport(socket);
				_lastPingTicks = Stopwatch.GetTimestamp();
			}

			public TimeSpan TimeSinceActivity => TimeSpan.FromTicks(Stopwatch.GetTimestamp() - Interlocked.CompareExchange(ref _lastPingTicks, 0, 0));

			public ValueTask MarkCompleteAsync(CancellationToken cancellationToken) => _inner.MarkCompleteAsync(cancellationToken);

			public async ValueTask<int> ReadPartialAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int result = await _inner.ReadPartialAsync(buffer, cancellationToken);
				if (result > 0)
				{
					Interlocked.Exchange(ref _lastPingTicks, Stopwatch.GetTimestamp());
				}
				return result;
			}

			public async ValueTask WriteAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken)
			{
				await _inner.WriteAsync(buffer, cancellationToken);
				Interlocked.Exchange(ref _lastPingTicks, Stopwatch.GetTimestamp());
			}
		}

		readonly ComputeListenerService _listenerService;
		readonly IMemoryCache _memoryCache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeHandler(ComputeListenerService listenerService, IMemoryCache memoryCache, ILogger<ComputeHandler> logger)
		{
			_listenerService = listenerService;
			_memoryCache = memoryCache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ComputeTask computeTask, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Starting compute task (lease {LeaseId}). Waiting for connection with nonce {Nonce}...", leaseId, StringUtils.FormatHexString(computeTask.Nonce.Span));

			TcpClient? tcpClient = null;
			try
			{
				const int TimeoutSeconds = 30;

				tcpClient = await _listenerService.WaitForClientAsync(new ByteString(computeTask.Nonce.Memory), TimeSpan.FromSeconds(TimeoutSeconds), cancellationToken);
				if (tcpClient == null)
				{
					_logger.LogInformation("Timed out waiting for connection after {Time}s.", TimeoutSeconds); 
					return LeaseResult.Success;
				}

				_logger.LogInformation("Matched connection for {Nonce}", StringUtils.FormatHexString(computeTask.Nonce.Span));

				TcpTransportWithTimeout transport = new TcpTransportWithTimeout(tcpClient.Client);
				using (CancellationTokenSource cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
				{
					await using BackgroundTask timeoutTask = BackgroundTask.StartNew(ctx => TickTimeoutAsync(transport, cts, ctx));
					await using (ComputeSocket socket = new ComputeSocket(transport, ComputeSocketEndpoint.Local, _logger))
					{
						DirectoryReference sandboxDir = DirectoryReference.Combine(session.WorkingDir, "Sandbox", leaseId);
						try
						{
							DirectoryReference.CreateDirectory(sandboxDir);

							AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, _memoryCache, _logger);
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
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while executing compute task: {Message}", ex.Message);
				return LeaseResult.Failed;
			}
			finally
			{
				tcpClient?.Dispose();
			}
		}

		async Task TickTimeoutAsync(TcpTransportWithTimeout transport, CancellationTokenSource cts, CancellationToken cancellationToken)
		{
			while(!cancellationToken.IsCancellationRequested)
			{
				if (transport.TimeSinceActivity > TimeSpan.FromMinutes(10))
				{
					_logger.LogWarning("Terminating compute task due to timeout (last tick at {Time})", DateTime.UtcNow - transport.TimeSinceActivity);
					cts.Cancel();
					break;
				}
				await Task.Delay(TimeSpan.FromSeconds(20), cancellationToken);
			}
		}
	}
}

