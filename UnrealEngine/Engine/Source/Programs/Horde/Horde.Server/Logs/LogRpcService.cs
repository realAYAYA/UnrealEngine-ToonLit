// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Grpc.Core;
using Horde.Common.Rpc;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Logging;

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
			ILogFile? logFile = await _logFileService.GetLogFileAsync(LogId.Parse(request.LogId), context.CancellationToken);
			if (logFile == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Resource not found");
			}
			if (!LogFileService.AuthorizeForSession(logFile, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Access denied");
			}

			using IStorageClient store = _storageService.CreateClient(Namespace.Logs);

			_logger.LogInformation("Updating {LogId} to node {RefTarget} (lines: {LineCount}, complete: {Complete})", request.LogId, request.TargetLocator, request.LineCount, request.Complete);

			IoHash hash;
			if (!IoHash.TryParse(request.TargetHash, out hash))
			{
				hash = IoHash.Zero;
			}

			IBlobRef target = store.CreateBlobRef(hash, new BlobLocator(request.TargetLocator));
			await store.WriteRefAsync(new RefName(request.LogId), target);

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
						await cancellationSource.CancelAsync();

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
