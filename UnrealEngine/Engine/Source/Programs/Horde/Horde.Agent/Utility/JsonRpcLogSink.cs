// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
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
			await _rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.CreateEventsAsync(new CreateEventsRequest(events)), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(WriteOutputRequest request, CancellationToken cancellationToken)
		{
			await _rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.WriteOutputAsync(request), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task SetOutcomeAsync(JobStepOutcome outcome, CancellationToken cancellationToken)
		{
			// Update the outcome of this jobstep
			if (_jobId != null && _jobBatchId != null && _jobStepId != null)
			{
				try
				{
					await _rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(_jobId, _jobBatchId, _jobStepId, JobStepState.Unspecified, outcome)), cancellationToken);
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
		readonly IStorageWriter _writer;
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
			_writer = store.CreateWriter();
			_logger = logger;

			_tailTaskStop = new AsyncEvent();
			_tailTask = Task.Run(() => TickTailAsync());
		}

		public async ValueTask DisposeAsync()
		{
			if (_tailTaskStop != null)
			{
				_tailTaskStop.Latch();
				_newTailDataEvent.Latch();

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
				await _writer.DisposeAsync();
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
					_logger.LogInformation("No tail data available for log {LogId} after {TailNext}; waiting for more...", _logId, tailNext);
					await newTailDataTask;
					continue;
				}

				string start = "";
				if (tailData.Length > 0)
				{
					start = Encoding.UTF8.GetString(tailData.Slice(0, Math.Min(tailData.Length, 256)).Span);
				}

				// Update the next tailing position
				int numLines = CountLines(tailData.Span);
				_logger.LogInformation("Setting log {LogId} tail = {TailNext}, data = {TailDataSize} bytes, {NumLines} lines ('{Start}')", _logId, tailNext, tailData.Length, numLines, start);

				int newTailNext = await UpdateLogTailAsync(tailNext, tailData);
				_logger.LogInformation("Log {LogId} tail next = {TailNext}", _logId, newTailNext);

				if (newTailNext != tailNext)
				{
					tailNext = newTailNext;
					_logger.LogInformation("Modified tail position for log {LogId} to {TailNext}", _logId, tailNext);
				}
			}
		}

		static int CountLines(ReadOnlySpan<byte> data)
		{
			int lines = 0;
			for (int idx = 0; idx < data.Length; idx++)
			{
				if (data[idx] == '\n')
				{
					lines++;
				}
			}
			return lines;
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
			_builder.WriteData(request.Data.Memory);
			_bufferLength += request.Data.Length;

			if (request.Flush || _bufferLength > FlushLength)
			{
				NodeRef<LogNode> target = await _builder.FlushAsync(_writer, request.Flush, cancellationToken);
				await UpdateLogAsync(target.Handle, _builder.LineCount, request.Flush, cancellationToken);
				_bufferLength = 0;
			}

			_newTailDataEvent.Set();
		}

		#region RPC calls

		protected virtual async Task UpdateLogAsync(BlobHandle target, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Updating log {LogId} to line {LineCount}, target {Locator}", _logId, lineCount, target);

			UpdateLogRequest request = new UpdateLogRequest();
			request.LogId = _logId;
			request.LineCount = lineCount;
			request.Target = target.GetLocator().ToString();
			request.Complete = complete;
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
					_logger.LogInformation("Writing log data: {LogId}, {TailNext}, {TailData} bytes", _logId, tailNext, tailData.Length);

					// Wait until the server responds or we need to trigger a new update
					Task<bool> moveNextAsync = call.ResponseStream.MoveNext();

					Task task = await Task.WhenAny(moveNextAsync, clientRef.DisposingTask, _tailTaskStop.Task, Task.Delay(TimeSpan.FromMinutes(1.0), CancellationToken.None));
					if (task == clientRef.DisposingTask)
					{
						_logger.LogInformation("Cancelling long poll from client side (server migration)");
					}
					else if (task == _tailTaskStop.Task)
					{
						_logger.LogInformation("Cancelling long poll from client side (complete)");
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
