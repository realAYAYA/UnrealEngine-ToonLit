// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.AspNetCore.Authorization;
using Horde.Common.Rpc;
using System.Threading.Tasks;
using Grpc.Core;
using Horde.Build.Utilities;
using EpicGames.Horde.Storage;
using Horde.Build.Storage;
using Microsoft.Extensions.Logging;
using System.Threading;

namespace Horde.Build.Logs
{
	using LogId = ObjectId<ILogFile>;

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
			ILogFile? logFile = await _logFileService.GetCachedLogFileAsync(new LogId(request.LogId), context.CancellationToken);
			if (logFile == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Resource not found");
			}
			if (!LogFileService.AuthorizeForSession(logFile, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Access denied");
			}

			IStorageClient store = await _storageService.GetClientAsync(Namespace.Logs, context.CancellationToken);
			_logger.LogInformation("Updating {LogId} to node {RefTarget}", request.LogId, request.Target);
			await store.WriteRefTargetAsync(new RefName(request.LogId), NodeHandle.Parse(request.Target));

			await _logFileCollection.UpdateLineCountAsync(logFile, request.LineCount, CancellationToken.None);

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
				LogId logId = new LogId(request.LogId);

				if (request.TailData.Length > 0)
				{
					await _logTailService.AppendAsync(logId, request.TailNext, request.TailData.Memory);
				}

				moveNextTask = requestStream.MoveNext();

				using (CancellationTokenSource cancellationSource = new CancellationTokenSource())
				{
					Task<int> waitTask = _logTailService.WaitForTailNextAsync(logId, cancellationSource.Token);

					await Task.WhenAny(waitTask, moveNextTask);
					cancellationSource.Cancel();

					response.TailNext = await waitTask;
				}

				await responseStream.WriteAsync(response);
			}

			await responseStream.WriteAsync(response);
		}
	}
}
