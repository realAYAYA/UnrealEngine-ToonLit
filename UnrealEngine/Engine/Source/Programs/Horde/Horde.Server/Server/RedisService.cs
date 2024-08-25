// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using StackExchange.Redis;

namespace Horde.Server.Server
{
	/// <summary>
	/// Manages the lifetime of a bundled Redis instance
	/// </summary>
	public sealed class RedisService : IHealthCheck, IAsyncDisposable
	{
		/// <summary>
		/// Default Redis port
		/// </summary>
		const int RedisPort = 6379;

		/// <summary>
		/// Connection pool
		/// </summary>
		public RedisConnectionPool ConnectionPool { get; }

		RedisProcess? _redisProcess;
		readonly ILogger<RedisService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public RedisService(IOptions<ServerSettings> options, ILogger<RedisService> logger)
			: this(options.Value.RedisConnectionConfig, -1, logger)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionString">Redis connection string. If null, we will start a temporary redis instance on the local machine.</param>
		/// <param name="dbNum">Override for the database to use. Set to -1 to use the default from the connection string.</param>
		/// <param name="logger"></param>
		public RedisService(string? connectionString, int dbNum, ILogger<RedisService> logger)
		{
			_logger = logger;

			if (connectionString == null)
			{
				if (IsRunningOnDefaultPort())
				{
					connectionString = $"127.0.0.1:{RedisPort}";
				}
				else if (TryStartRedisProcess())
				{
					connectionString = $"127.0.0.1:{_redisProcess!.Port},allowAdmin=true";
				}
				else
				{
					throw new Exception($"Unable to connect to Redis. Please set {nameof(ServerSettings.RedisConnectionConfig)} in {ServerApp.ServerConfigFile}");
				}
			}

			ConnectionPool = new RedisConnectionPool(20, connectionString, dbNum);
		}

		/// <inheritdoc/>
		public async Task<HealthCheckResult> CheckHealthAsync(HealthCheckContext context, CancellationToken cancellationToken = default)
		{
			try
			{
				TimeSpan span = await GetDatabase().PingAsync();
				return HealthCheckResult.Healthy(data: new Dictionary<string, object> { ["Latency"] = span.ToString() });
			}
			catch (Exception ex)
			{
				return HealthCheckResult.Unhealthy("Unable to ping Redis", ex);
			}
		}

		/// <summary>
		/// Get the least-loaded Redis connection from the pool
		/// Don't store the returned object and try to resolve this as late as possible to ensure load is balanced.
		/// </summary>
		/// <returns>A Redis connection multiplexer</returns>
		public IConnectionMultiplexer GetConnection()
		{
			return ConnectionPool.GetConnection();
		}

		/// <summary>
		/// Get the least-loaded Redis database from the connection pool
		/// Don't store the returned object and try to resolve this as late as possible to ensure load is balanced.
		/// </summary>
		/// <returns>A Redis database</returns>
		public IDatabase GetDatabase()
		{
			return ConnectionPool.GetDatabase();
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_redisProcess != null)
			{
				_logger.LogInformation("Sending shutdown command...");
				ConnectionPool.GetConnection().GetServers().FirstOrDefault()?.Shutdown();
			}

			ConnectionPool.Dispose();

			if (_redisProcess != null)
			{
				await _redisProcess.DisposeAsync();
				_redisProcess = null;
			}
		}

		/// <summary>
		/// Checks if Redis is already running on the default port
		/// </summary>
		/// <returns></returns>
		static bool IsRunningOnDefaultPort()
		{
			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();

			IPEndPoint[] listeners = ipGlobalProperties.GetActiveTcpListeners();
			if (listeners.Any(x => x.Port == RedisPort))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Attempts to start a local instance of Redis
		/// </summary>
		/// <returns></returns>
		bool TryStartRedisProcess()
		{
			if (_redisProcess != null)
			{
				return true;
			}
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return false;
			}

			FileReference redisConfigFile = FileReference.Combine(RedisProcess.RedisExe.Directory, "redis.conf");
			_redisProcess = new RedisProcess(_logger);
			_redisProcess.Start($"\"{redisConfigFile}\"");
			return true;
		}

		/// <summary>
		/// Publish a message to a channel
		/// </summary>
		/// <param name="channel">Channel to post to</param>
		/// <param name="message">Message to post to the channel</param>
		/// <param name="flags">Flags for the request</param>
		public Task PublishAsync(RedisChannel channel, RedisValue message, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().PublishAsync(channel, message, flags);
		}

		/// <summary>
		/// Publish a message to a channel
		/// </summary>
		/// <typeparam name="T">Type of elements sent over the channel</typeparam>
		/// <param name="channel">Channel to post to</param>
		/// <param name="message">Message to post to the channel</param>
		/// <param name="flags">Flags for the request</param>
		public Task PublishAsync<T>(RedisChannel<T> channel, T message, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().PublishAsync(channel, message, flags);
		}

		/// <inheritdoc cref="SubscribeAsync{T}(RedisChannel{T}, Action{RedisChannel{T}, T})"/>
		public Task<RedisSubscription> SubscribeAsync(RedisChannel channel, Action<RedisValue> callback) => SubscribeAsync(channel, (ch, x) => callback(x));

		/// <inheritdoc cref="SubscribeAsync{T}(RedisChannel{T}, Action{RedisChannel{T}, T})"/>
		public Task<RedisSubscription> SubscribeAsync<T>(RedisChannel<T> channel, Action<T> callback) => SubscribeAsync(channel, (ch, x) => callback(x));

		/// <summary>
		/// Subscribe to notifications on a channel
		/// </summary>
		/// <param name="channel">Channel to monitor</param>
		/// <param name="callback">Callback for new events</param>
		/// <returns>Subscription object</returns>
		public async Task<RedisSubscription> SubscribeAsync(RedisChannel channel, Action<RedisChannel, RedisValue> callback)
		{
			IConnectionMultiplexer connection = GetConnection();
			return await connection.SubscribeAsync(channel, callback);
		}

		/// <summary>
		/// Subscribe to notifications on a channel
		/// </summary>
		/// <typeparam name="T">Type of elements sent over the channel</typeparam>
		/// <param name="channel">Channel to monitor</param>
		/// <param name="callback">Callback for new events</param>
		/// <returns>Subscription object</returns>
		public async Task<RedisSubscription> SubscribeAsync<T>(RedisChannel<T> channel, Action<RedisChannel<T>, T> callback)
		{
			IConnectionMultiplexer connection = GetConnection();
			return await connection.SubscribeAsync(channel, callback);
		}
	}
}
