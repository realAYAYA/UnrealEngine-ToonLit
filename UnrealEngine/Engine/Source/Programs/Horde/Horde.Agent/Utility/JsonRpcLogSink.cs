// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
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

	class JsonRpcAndStorageLogSink : IJsonRpcLogSink, IAsyncDisposable
	{
		const int FlushLength = 1024 * 1024;

		readonly IRpcConnection _connection;
		readonly JobId? _jobId;
		readonly JobStepBatchId? _jobBatchId;
		readonly JobStepId? _jobStepId;
		readonly LogId _logId;
		readonly LogBuilder _builder;
		readonly IStorageClient _store;
		readonly IBlobWriter _writer;
		readonly ILogger _logger;

		int _bufferLength;

		// Tailing task
		readonly Task _tailTask;
		AsyncEvent _tailTaskStop;
		readonly AsyncEvent _newTailDataEvent = new AsyncEvent();

		public JsonRpcAndStorageLogSink(IRpcConnection connection, LogId logId, JobId? jobId, JobStepBatchId? jobBatchId, JobStepId? jobStepId, IStorageClient store, ILogger logger)
		{
			_connection = connection;
			_logId = logId;
			_jobId = jobId;
			_jobBatchId = jobBatchId;
			_jobStepId = jobStepId;
			_builder = new LogBuilder(LogFormat.Json, logger);
			_store = store;
			_writer = store.CreateBlobWriter();
			_logger = logger;

			_tailTaskStop = new AsyncEvent();
			_tailTask = Task.Run(() => TickTailAsync());
		}

		public async ValueTask DisposeAsync()
		{
			_logger.LogInformation("Disposing json log task");

			if (_tailTaskStop != null)
			{
				_tailTaskStop.Latch();
				_newTailDataEvent.Latch();

				await _tailTask;
				_tailTaskStop = null!;
			}

			if (_writer != null)
			{
				await _writer.DisposeAsync();
			}

			if (_store != null)
			{
				_store.Dispose();
			}
		}

		async Task TickTailAsync()
		{
			for (; ; )
			{
				try
				{
					await TickTailInternalAsync();
					break;
				}
				catch (OperationCanceledException ex)
				{
					_logger.LogInformation(ex, "Cancelled log tailing task");
					break;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception on log tailing task ({LogId}): {Message}", _logId, ex.Message);
					await Task.Delay(TimeSpan.FromSeconds(10.0));
				}
			}
		}

		async Task TickTailInternalAsync()
		{
			int tailNext = -1;
			while (!_tailTaskStop.IsSet())
			{
				Task newTailDataTask = _newTailDataEvent.Task;
				int initialTailNext = tailNext;

				// Get the data to send to the server
				ReadOnlyMemory<byte> tailData = ReadOnlyMemory<byte>.Empty;
				if (tailNext != -1)
				{
					(tailNext, tailData) = _builder.ReadTailData(tailNext, 16 * 1024);
				}

				// If we don't have any updates for the server, wait until we do.
				if (tailNext != -1 && tailData.IsEmpty && tailNext == initialTailNext)
				{
					_logger.LogInformation("No tail data available for log {LogId} after line {TailNext}; waiting for more...", _logId, tailNext);
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

				int newTailNext = await UpdateLogTailAsync(tailNext, tailData, CancellationToken.None);
				_logger.LogInformation("Log {LogId} tail next = {TailNext}", _logId, newTailNext);

				if (newTailNext != tailNext)
				{
					tailNext = newTailNext;
					_logger.LogInformation("Modified tail position for log {LogId} to {TailNext}", _logId, tailNext);
				}
			}
			_logger.LogInformation("Finishing log tail task");
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
			// Update the outcome of this jobstep
			if (_jobId != null && _jobBatchId != null && _jobStepId != null)
			{
				try
				{
					await _connection.InvokeAsync((JobRpc.JobRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(_jobId.Value, _jobBatchId.Value, _jobStepId.Value, JobStepState.Unspecified, outcome)), cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", outcome);
				}
			}
		}

		/// <inheritdoc/>
		public async Task WriteEventsAsync(List<CreateEventRequest> events, CancellationToken cancellationToken)
		{
			await _connection.InvokeAsync((JobRpc.JobRpcClient x) => x.CreateEventsAsync(new CreateEventsRequest(events)), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task WriteOutputAsync(WriteOutputRequest request, CancellationToken cancellationToken)
		{
			_builder.WriteData(request.Data.Memory);
			_bufferLength += request.Data.Length;

			if (request.Flush || _bufferLength > FlushLength)
			{
				IBlobRef<LogNode> target = await _builder.FlushAsync(_writer, request.Flush, cancellationToken);
				await UpdateLogAsync(target, _builder.LineCount, request.Flush, cancellationToken);
				_bufferLength = 0;
			}

			_newTailDataEvent.Set();
		}

		#region RPC calls

		protected virtual async Task UpdateLogAsync(IBlobRef target, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Updating log {LogId} to line {LineCount}, target {Locator}", _logId, lineCount, target.GetLocator());

			UpdateLogRequest request = new UpdateLogRequest();
			request.LogId = _logId.ToString();
			request.LineCount = lineCount;
			request.TargetHash = target.Hash.ToString();
			request.TargetLocator = target.GetLocator().ToString();
			request.Complete = complete;
			await _connection.InvokeAsync((LogRpcClient client) => client.UpdateLogAsync(request, cancellationToken: cancellationToken), cancellationToken);
		}

		protected virtual async Task<int> UpdateLogTailAsync(int tailNext, ReadOnlyMemory<byte> tailData, CancellationToken cancellationToken)
		{
			DateTime deadline = DateTime.UtcNow.AddMinutes(2.0);
			try
			{
				using IRpcClientRef<LogRpcClient> clientRef = await _connection.GetClientRefAsync<LogRpcClient>(cancellationToken);
				using AsyncDuplexStreamingCall<UpdateLogTailRequest, UpdateLogTailResponse> call = clientRef.Client.UpdateLogTail(deadline: deadline, cancellationToken: cancellationToken);

				// Write the request to the server
				UpdateLogTailRequest request = new UpdateLogTailRequest();
				request.LogId = _logId.ToString();
				request.TailNext = tailNext;
				request.TailData = UnsafeByteOperations.UnsafeWrap(tailData);
				await call.RequestStream.WriteAsync(request, cancellationToken);
				_logger.LogInformation("Writing log data: {LogId}, {TailNext}, {TailData} bytes", _logId, tailNext, tailData.Length);

				// Wait until the server responds or we need to trigger a new update
				Task<bool> moveNextAsync = call.ResponseStream.MoveNext();

				Task task = await Task.WhenAny(moveNextAsync, clientRef.DisposingTask, _tailTaskStop.Task, Task.Delay(TimeSpan.FromMinutes(1.0), CancellationToken.None));
				if (task == clientRef.DisposingTask)
				{
					TimeSpan graceDelay = TimeSpan.FromSeconds(10);
					_logger.LogInformation("Cancelling long poll from client side (server migration). Backing off for {Delay} ms...", graceDelay.TotalMilliseconds);
					await Task.Delay(graceDelay, cancellationToken);
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
			catch (RpcException ex) when (ex.StatusCode == StatusCode.DeadlineExceeded)
			{
				_logger.LogDebug(ex, "Log tail deadline exceeded, ignoring.");
				return -1;
			}
		}

		#endregion
	}
}
