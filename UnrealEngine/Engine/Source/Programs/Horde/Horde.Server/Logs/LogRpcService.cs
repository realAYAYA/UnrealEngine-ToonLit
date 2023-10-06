// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.AspNetCore.Authorization;
using Horde.Common.Rpc;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Server.Utilities;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using Microsoft.Extensions.Logging;
using System.Threading;
using System;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Implements the Horde gRPC service for bots updating their status and dequeing work
	/// </summary>
	[Authorize]
	public class LogRpcService : LogRpc.LogRpcBase
	{
		readonly ILogFileService _logFileService;
		readonly ILogFileCollection _logFileCollection;
		readonly LogTailService _logTailService;
		readonly StorageService _storageService;
		readonly ILogger<LogRpcService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public LogRpcService(ILogFileService logFileService, ILogFileCollection logFileCollection, LogTailService logTailService, StorageService storageService, ILogger<LogRpcService> logger)
		{
			_logFileService = logFileService;
			_logFileCollection = logFileCollection;
			_logTailService = logTailService;
			_storageService = storageService;
			_logger = logger;
		}

		/// <inheritdoc/>
		public override async Task<UpdateLogResponse> UpdateLog(UpdateLogRequest request, ServerCallContext context)
		{
			ILogFile? logFile = await _logFileService.GetCachedLogFileAsync(LogId.Parse(request.LogId), context.CancellationToken);
			if (logFile == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Resource not found");
			}
			if (!LogFileService.AuthorizeForSession(logFile, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Access denied");
			}

			IStorageClientImpl store = await _storageService.GetClientAsync(Namespace.Logs, context.CancellationToken);
			_logger.LogInformation("Updating {LogId} to node {RefTarget} (lines: {LineCount}, complete: {Complete})", request.LogId, request.Target, request.LineCount, request.Complete);
			await store.WriteRefTargetAsync(new RefName(request.LogId), NodeLocator.Parse(request.Target));

			await _logFileCollection.UpdateLineCountAsync(logFile, request.LineCount, request.Complete, CancellationToken.None);

			await _logTailService.FlushAsync(logFile.Id, request.LineCount);

			return new UpdateLogResponse();
		}

		/// <inheritdoc/>
		public override async Task UpdateLogTail(IAsyncStreamReader<UpdateLogTailRequest> requestStream, IServerStreamWriter<UpdateLogTailResponse> responseStream, ServerCallContext context)
		{
			UpdateLogTailResponse response = new UpdateLogTailResponse();
			response.TailNext = -1;

			Task<bool> moveNextTask = requestStream.MoveNext();
			while (await moveNextTask)
			{
				UpdateLogTailRequest request = requestStream.Current;
				LogId logId = LogId.Parse(request.LogId);
				_logger.LogDebug("Updating log tail for {LogId}: line {LineIdx}, size {Size}", logId, request.TailNext, request.TailData.Length);

				if (request.TailData.Length > 0)
				{
					await _logTailService.AppendAsync(logId, request.TailNext, request.TailData.Memory);
				}

				moveNextTask = requestStream.MoveNext();

				response.TailNext = await _logTailService.GetTailNextAsync(logId, context.CancellationToken);
				if (response.TailNext == -1)
				{
					using (CancellationTokenSource cancellationSource = new CancellationTokenSource())
					{
						_logger.LogDebug("Waiting for tail next on log {LogId}", logId);
						Task<int> waitTask = _logTailService.WaitForTailNextAsync(logId, cancellationSource.Token);

						await Task.WhenAny(waitTask, moveNextTask);
						cancellationSource.Cancel();

						try
						{
							response.TailNext = await waitTask;
						}
						catch (Exception ex)
						{
							_logger.LogWarning(ex, "Exception while waiting for tail next");
						}
					}
				}

				_logger.LogDebug("Return tail next for log {LogId} = {TailNext}", logId, response.TailNext);
				await responseStream.WriteAsync(response);
			}

			await responseStream.WriteAsync(response);
		}
	}
}
