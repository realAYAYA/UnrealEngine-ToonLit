// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Interface for creating <see cref="IServerLogger"/> instances
	/// </summary>
	interface IServerLoggerFactory
	{
		/// <summary>
		/// Creates a logger which uploads data to the server
		/// </summary>
		/// <param name="session">The current session</param>
		/// <param name="logId">The log id</param>
		/// <param name="localLogger">Local log output device</param>
		/// <param name="jobId">Job containing the log</param>
		/// <param name="batchId">Job batch id</param>
		/// <param name="stepId">The job step id</param>
		/// <param name="warnings">Whether to suppress warnings</param>
		/// <param name="outputLevel">Minimum output level for messages</param>
		/// <returns>New logger instance</returns>
		IServerLogger CreateLogger(ISession session, LogId logId, ILogger localLogger, JobId? jobId, JobStepBatchId? batchId, JobStepId? stepId, bool? warnings, LogLevel outputLevel = LogLevel.Information);
	}

	/// <summary>
	/// Extension methods for <see cref="IServerLoggerFactory"/>
	/// </summary>
	static class ServerLoggerFactoryExtensions
	{
		/// <summary>
		/// Creates a logger which uploads data to the server
		/// </summary>
		/// <param name="service">Service instance</param>
		/// <param name="session">The current session</param>
		/// <param name="logId">The log identifier</param>
		/// <param name="localLogger">Local log output device</param>
		/// <param name="warnings">Whether to suppress warnings</param>
		/// <param name="outputLevel">Minimum output level for messages</param>
		/// <returns>New logger instance</returns>
		public static IServerLogger CreateLogger(this IServerLoggerFactory service, ISession session, LogId logId, ILogger localLogger, bool? warnings, LogLevel outputLevel = LogLevel.Information)
		{
			return service.CreateLogger(session, logId, localLogger, null, null, null, warnings, outputLevel);
		}
	}

	/// <summary>
	/// Implementation of <see cref="IServerLoggerFactory"/>
	/// </summary>
	class ServerLoggerFactory : IServerLoggerFactory
	{
		readonly HttpStorageClientFactory _storageClientFactory;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerLoggerFactory(HttpStorageClientFactory storageClientFactory, ILogger<ServerLoggerFactory> logger)
		{
			_storageClientFactory = storageClientFactory;
			_logger = logger;
		}

		/// <inheritdoc/>
		public IServerLogger CreateLogger(ISession session, LogId logId, ILogger localLogger, JobId? jobId, JobStepBatchId? batchId, JobStepId? stepId, bool? warnings, LogLevel outputLevel)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			IStorageClient storageClient = _storageClientFactory.CreateClientWithPath($"api/v1/logs/{logId}", session.Token);
			IJsonRpcLogSink sink = new JsonRpcAndStorageLogSink(session.RpcConnection, logId, jobId, batchId, stepId, storageClient, _logger);

			return new ServerLogger(sink, logId, warnings, outputLevel, localLogger, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
