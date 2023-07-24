// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Horde.Agent.Parser;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Backends;
using System.Net.Http;
using Microsoft.Extensions.Options;
using Horde.Agent.Utility;

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
		/// <returns>New logger instance</returns>
		IServerLogger CreateLogger(ISession session, string logId, string? jobId, string? batchId, string? stepId, bool? warnings = null);
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
		/// <returns>New logger instance</returns>
		public static IServerLogger CreateLogger(this IServerLoggerFactory service, ISession session, string logId, bool? warnings = null)
		{
			return service.CreateLogger(session, logId, null, null, null, warnings);
		}
	}

	/// <summary>
	/// Implementation of <see cref="IServerLoggerFactory"/>
	/// </summary>
	class ServerLoggerFactory : IServerLoggerFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly IOptions<AgentSettings> _settings;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerLoggerFactory(IHttpClientFactory httpClientFactory, IOptions<AgentSettings> settings, ILogger<ServerLoggerFactory> logger)
		{
			_httpClientFactory = httpClientFactory;
			_settings = settings;
			_logger = logger;
		}

		/// <inheritdoc/>
		public IServerLogger CreateLogger(ISession session, string logId, string? jobId, string? batchId, string? stepId, bool? warnings = null)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			IJsonRpcLogSink sink = new JsonRpcLogSink(session.RpcConnection, jobId, batchId, stepId, _logger);
			if (_settings.Value.EnableNewLogger)
			{
				HttpStorageClient storageClient = new HttpStorageClient(_httpClientFactory,  new Uri(session.ServerUrl, $"api/v1/logs/{logId}/"), session.Token, _logger);
				sink = new JsonRpcAndStorageLogSink(session.RpcConnection, logId, sink, storageClient, _logger);
			}
			return new JsonRpcLogger(sink, logId, warnings, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
