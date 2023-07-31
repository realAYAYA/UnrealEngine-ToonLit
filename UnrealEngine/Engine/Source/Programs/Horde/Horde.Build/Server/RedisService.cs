// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using StackExchange.Redis;

namespace Horde.Build.Server
{
	/// <summary>
	/// Manages the lifetime of a bundled Redis instance
	/// </summary>
	public sealed class RedisService : IDisposable
	{
		/// <summary>
		/// Default Redis port
		/// </summary>
		const int RedisPort = 6379;

		/// <summary>
		/// The managed process group containing the Redis server
		/// </summary>
		ManagedProcessGroup? _redisProcessGroup;

		/// <summary>
		/// The server process
		/// </summary>
		ManagedProcess? _redisProcess;

		/// <summary>
		/// Connection multiplexer
		/// </summary>
		private readonly ConnectionMultiplexer _multiplexer;

		/// <summary>
		/// The database interface.
		/// If possible, use ConnectionPool instead. 
		/// </summary>
		public IDatabase Database { get; }

		/// <summary>
		/// Connection pool
		/// </summary>
		public RedisConnectionPool ConnectionPool { get; }

		/// <summary>
		/// Logger factory
		/// </summary>
		readonly ILoggerFactory _loggerFactory;

		/// <summary>
		/// Logging instance
		/// </summary>
		readonly ILogger<RedisService> _logger;

		/// <summary>
		/// Hack to initialize RedisService early enough to use data protection
		/// </summary>
		/// <param name="settings"></param>
		public RedisService(ServerSettings settings)
			: this(Options.Create(settings), new Serilog.Extensions.Logging.SerilogLoggerFactory(Serilog.Log.Logger))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public RedisService(IOptions<ServerSettings> options, ILoggerFactory loggerFactory)
			: this(options.Value.RedisConnectionConfig, -1, loggerFactory)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionString">Redis connection string. If null, we will start a temporary redis instance on the local machine.</param>
		/// <param name="dbNum">Override for the database to use. Set to -1 to use the default from the connection string.</param>
		/// <param name="loggerFactory"></param>
		public RedisService(string? connectionString, int dbNum, ILoggerFactory loggerFactory)
		{
			_loggerFactory = loggerFactory;
			_logger = loggerFactory.CreateLogger<RedisService>();

			if (connectionString == null)
			{
				if (IsRunningOnDefaultPort())
				{
					connectionString = $"localhost:{RedisPort}";
				}
				else if (TryStartRedisServer())
				{
					connectionString = $"localhost:{RedisPort}";
				}
				else
				{
					throw new Exception($"Unable to connect to Redis. Please set {nameof(ServerSettings.RedisConnectionConfig)} in {Program.UserConfigFile}");
				}
			}

			_multiplexer = ConnectionMultiplexer.Connect(connectionString);
			Database = _multiplexer.GetDatabase(dbNum);
			ConnectionPool = new RedisConnectionPool(20, connectionString, dbNum);
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
		public void Dispose()
		{
			_multiplexer.Dispose();

			if (_redisProcess != null)
			{
				_redisProcess.Dispose();
				_redisProcess = null;
			}
			if (_redisProcessGroup != null)
			{
				_redisProcessGroup.Dispose();
				_redisProcessGroup = null;
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
		bool TryStartRedisServer()
		{
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return false;
			}

			FileReference redisExe = FileReference.Combine(Program.AppDir, "ThirdParty", "Redis", "redis-server.exe");
			if (!FileReference.Exists(redisExe))
			{
				_logger.LogDebug("Redis executable does not exist at {ExePath}", redisExe);
				return false;
			}

			DirectoryReference redisDir = DirectoryReference.Combine(Program.DataDir, "Redis");
			DirectoryReference.CreateDirectory(redisDir);

			FileReference redisConfigFile = FileReference.Combine(redisDir, "redis.conf");
			if (!FileReference.Exists(redisConfigFile))
			{
				using (StreamWriter writer = new StreamWriter(redisConfigFile.FullName))
				{
					writer.WriteLine("# redis.conf");
				}
			}

			_redisProcessGroup = new ManagedProcessGroup();
			try
			{
				_redisProcess = new ManagedProcess(_redisProcessGroup, redisExe.FullName, "", null, null, ProcessPriorityClass.Normal);
				_redisProcess.StdIn.Close();
				Task.Run(() => RelayRedisOutput());
				return true;
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Unable to start Redis server process");
				return false;
			}
		}

		/// <summary>
		/// Copies output from the redis process to the logger
		/// </summary>
		/// <returns></returns>
		async Task RelayRedisOutput()
		{
			ILogger redisLogger = _loggerFactory.CreateLogger("Redis");
			for (; ; )
			{
				string? line = await _redisProcess!.ReadLineAsync();
				if (line == null)
				{
					break;
				}
				if (line.Length > 0)
				{
					redisLogger.Log(LogLevel.Information, "{Output}", line);
				}
			}
			redisLogger.LogInformation("Exit code {ExitCode}", _redisProcess.ExitCode);
		}
	}
}
