// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using Horde.Agent.Utility;
using EpicGames.Horde.Storage;

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
		/// <param name="jobId">Job containing the log</param>
		/// <param name="batchId">Job batch id</param>
		/// <param name="stepId">The job step id</param>
		/// <param name="warnings">Whether to suppress warnings</param>
		/// <param name="useNewLogger">Whether to enable the new logger backend</param>
		/// <returns>New logger instance</returns>
		IServerLogger CreateLogger(ISession session, string logId, string? jobId, string? batchId, string? stepId, bool? warnings, bool? useNewLogger);
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
		/// <param name="warnings">Whether to suppress warnings</param>
		/// <param name="useNewLogger">Whether to enable the new logger backend</param>
		/// <returns>New logger instance</returns>
		public static IServerLogger CreateLogger(this IServerLoggerFactory service, ISession session, string logId, bool? warnings, bool? useNewLogger)
		{
			return service.CreateLogger(session, logId, null, null, null, warnings, useNewLogger);
		}
	}

	/// <summary>
	/// Implementation of <see cref="IServerLoggerFactory"/>
	/// </summary>
	class ServerLoggerFactory : IServerLoggerFactory
	{
		readonly IServerStorageFactory _storageClientFactory;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerLoggerFactory(IServerStorageFactory storageClientFactory, ILogger<ServerLoggerFactory> logger)
		{
			_storageClientFactory = storageClientFactory;
			_logger = logger;
		}

		/// <inheritdoc/>
		public IServerLogger CreateLogger(ISession session, string logId, string? jobId, string? batchId, string? stepId, bool? warnings, bool? useNewLogger)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			IJsonRpcLogSink sink = new JsonRpcLogSink(session.RpcConnection, jobId, batchId, stepId, _logger);
			if (useNewLogger ?? false)
			{
				IStorageClient storageClient = _storageClientFactory.CreateStorageClient(session, $"/api/v1/logs/{logId}/");
				sink = new JsonRpcAndStorageLogSink(session.RpcConnection, logId, sink, storageClient, _logger);
			}
			return new JsonRpcLogger(sink, logId, warnings, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
