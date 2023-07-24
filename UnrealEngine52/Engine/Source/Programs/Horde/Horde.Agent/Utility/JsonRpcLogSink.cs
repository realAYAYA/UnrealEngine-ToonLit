// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Google.Protobuf;
using Grpc.Core;
using Horde.Common.Rpc;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;
using static Horde.Common.Rpc.LogRpc;

namespace Horde.Agent.Utility
{
	interface IJsonRpcLogSink : IAsyncDisposable
	{
		Task WriteEventsAsync(List<CreateEventRequest> events, CancellationToken cancellationToken);
		Task WriteOutputAsync(WriteOutputRequest request, CancellationToken cancellationToken);
		Task SetOutcomeAsync(JobStepOutcome outcome, CancellationToken cancellationToken);
	}

	sealed class JsonRpcLogSink : IJsonRpcLogSink
	{
		readonly IRpcConnection _rpcClient;
		readonly string? _jobId;
		readonly string? _jobBatchId;
		readonly string? _jobStepId;
		readonly ILogger _logger;

		public JsonRpcLogSink(IRpcConnection rpcClient, string? jobId, string? jobBatchId, string? jobStepId, ILogger logger)
		{
			_rpcClient = rpcClient;
			_jobId = jobId;
			_jobBatchId = jobBatchId;
			_jobStepId = jobStepId;
			_logger = logger;
		}

		public ValueTask DisposeAsync() => new ValueTask();

		/// <inheritdoc/>
		public async Task WriteEventsAsync(List<CreateEventRequest> events, CancellationToken cancellationToken)
		{
			await _rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.CreateEventsAsync(new CreateEventsRequest(events)), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(WriteOutputRequest request, CancellationToken cancellationToken)
		{
			await _rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.WriteOutputAsync(request), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task SetOutcomeAsync(JobStepOutcome outcome, CancellationToken cancellationToken)
		{
			// Update the outcome of this jobstep
			if (_jobId != null && _jobBatchId != null && _jobStepId != null)
			{
				try
				{
					await _rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(_jobId, _jobBatchId, _jobStepId, JobStepState.Unspecified, outcome)), cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", outcome);
				}
			}
		}
	}

	class JsonRpcAndStorageLogSink : IJsonRpcLogSink, IAsyncDisposable
	{
		const int FlushLength = 1024 * 1024;

		readonly IRpcConnection _connection;
		readonly string _logId;
		readonly LogBuilder _builder;
		readonly IJsonRpcLogSink? _inner;
		readonly TreeWriter _writer;
		readonly ILogger _logger;

		int _bufferLength;

		// Background task
		readonly object _lockObject = new object();

		// Tailing task
		readonly Task _tailTask;
		AsyncEvent _tailTaskStop;
		readonly AsyncEvent _newTailDataEvent = new AsyncEvent();

		public JsonRpcAndStorageLogSink(IRpcConnection connection, string logId, IJsonRpcLogSink? inner, IStorageClient store, ILogger logger)
		{
			_connection = connection;
			_logId = logId;
			_builder = new LogBuilder(LogFormat.Json, logger);
			_inner = inner;
			_writer = new TreeWriter(store);
			_logger = logger;

			_tailTaskStop = new AsyncEvent();
			_tailTask = Task.Run(() => TickTailAsync());
		}

		public async ValueTask DisposeAsync()
		{
			if (_tailTaskStop != null)
			{
				_tailTaskStop.Latch();
				try
				{
					await _tailTask;
				}
				catch (OperationCanceledException)
				{
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception on log tailing task ({LogId}): {Message}", _logId, ex.Message);
				}
				_tailTaskStop = null!;
			}

			if (_inner != null)
			{
				await _inner.DisposeAsync();
			}

			if (_writer != null)
			{
				_writer.Dispose();
			}
		}

		async Task TickTailAsync()
		{
			int tailNext = -1;
			while (!_tailTaskStop.IsSet())
			{
				Task newTailDataTask;

				// Get the data to send to the server
				ReadOnlyMemory<byte> tailData = ReadOnlyMemory<byte>.Empty;
				lock (_lockObject)
				{
					if (tailNext != -1)
					{
						tailNext = Math.Max(tailNext, _builder.FlushedLineCount);
						tailData = _builder.ReadTailData(tailNext, 16 * 1024);
					}
					newTailDataTask = _newTailDataEvent.Task;
				}

				// If we don't have any updates for the server, wait until we do.
				if (tailNext != -1 && tailData.IsEmpty)
				{
					await newTailDataTask;
					continue;
				}

				// Update the next tailing position
				int newTailNext = await UpdateLogTailAsync(tailNext, tailData);
				if (newTailNext != tailNext)
				{
					tailNext = newTailNext;
					_logger.LogInformation("Modified tail position for log {LogId} to {TailNext}", _logId, tailNext);
				}
			}
		}

		/// <inheritdoc/>
		public async Task SetOutcomeAsync(JobStepOutcome outcome, CancellationToken cancellationToken)
		{
			if (_inner != null)
			{
				await _inner.SetOutcomeAsync(outcome, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task WriteEventsAsync(List<CreateEventRequest> events, CancellationToken cancellationToken)
		{
			if (_inner != null)
			{
				await _inner.WriteEventsAsync(events, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(WriteOutputRequest request, CancellationToken cancellationToken)
		{
			if (_inner != null)
			{
				await _inner.WriteOutputAsync(request, cancellationToken);
			}

			_builder.WriteData(request.Data.Memory);
			_bufferLength += request.Data.Length;

			if (request.Flush || _bufferLength > FlushLength)
			{
				NodeHandle target = await _builder.FlushAsync(_writer, request.Flush, cancellationToken);
				await UpdateLogAsync(target, _builder.LineCount, cancellationToken);
				_bufferLength = 0;
			}

			_newTailDataEvent.Set();
		}

		#region RPC calls

		protected virtual async Task UpdateLogAsync(NodeHandle target, int lineCount, CancellationToken cancellationToken)
		{
			_logger.LogDebug("Updating log {LogId} to line {LineCount}, target {Locator}", _logId, lineCount, target);

			UpdateLogRequest request = new UpdateLogRequest();
			request.LogId = _logId;
			request.LineCount = lineCount;
			request.Target = target.ToString();
			await _connection.InvokeAsync((LogRpcClient client) => client.UpdateLogAsync(request, cancellationToken: cancellationToken), cancellationToken);
		}

		protected virtual async Task<int> UpdateLogTailAsync(int tailNext, ReadOnlyMemory<byte> tailData)
		{
			DateTime deadline = DateTime.UtcNow.AddMinutes(2.0);
			using (IRpcClientRef<LogRpcClient> clientRef = await _connection.GetClientRefAsync<LogRpcClient>(CancellationToken.None))
			{
				using (AsyncDuplexStreamingCall<UpdateLogTailRequest, UpdateLogTailResponse> call = clientRef.Client.UpdateLogTail(deadline: deadline))
				{
					// Write the request to the server
					UpdateLogTailRequest request = new UpdateLogTailRequest();
					request.LogId = _logId;
					request.TailNext = tailNext;
					request.TailData = UnsafeByteOperations.UnsafeWrap(tailData);
					await call.RequestStream.WriteAsync(request);

					// Wait until the server responds or we need to trigger a new update
					Task<bool> moveNextAsync = call.ResponseStream.MoveNext();

					Task task = await Task.WhenAny(moveNextAsync, clientRef.DisposingTask, _tailTaskStop.Task, Task.Delay(TimeSpan.FromMinutes(1.0), CancellationToken.None));
					if (task == clientRef.DisposingTask)
					{
						_logger.LogDebug("Cancelling long poll from client side (server migration)");
					}
					else if (task == _tailTaskStop.Task)
					{
						_logger.LogDebug("Cancelling long poll from client side (complete)");
					}

					// Close the request stream to indicate that we're finished
					await call.RequestStream.CompleteAsync();

					// Wait for a response or a new update to come in, then close the request stream
					UpdateLogTailResponse? response = null;
					while (await moveNextAsync)
					{
						response = call.ResponseStream.Current;
						moveNextAsync = call.ResponseStream.MoveNext();
					}
					return response?.TailNext ?? -1;
				}
			}
		}

		#endregion
	}
}
