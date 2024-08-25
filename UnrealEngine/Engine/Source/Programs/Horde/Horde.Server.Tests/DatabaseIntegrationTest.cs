// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using StackExchange.Redis;

namespace Horde.Server.Tests
{
	// Stub for fulfilling IOptions interface during testing
	public sealed class TestOptions<T> : IOptions<T> where T : class
	{
		public TestOptions(T options) => Value = options;
		public T Value { get; }
	}

	// Stub for fulfilling IOptionsMonitor interface during testing
	public sealed class TestOptionsMonitor<T> : IOptionsMonitor<T>
		where T : class, new()
	{
		sealed class Disposable : IDisposable
		{
			public void Dispose() { }
		}

		public TestOptionsMonitor(T currentValue)
		{
			CurrentValue = currentValue;
		}

		public T Get(string? name)
		{
			return CurrentValue;
		}

		public IDisposable OnChange(Action<T, string> listener)
		{
			return new Disposable();
		}

		public T CurrentValue { get; }
	}

	public sealed class MongoInstance : IDisposable
	{
		public string DatabaseName { get; }
		public string ConnectionString { get; }
		MongoClient Client { get; }

		private static readonly object s_lockObject = new object();
		private static MongoDbRunnerLocal? s_mongoDbRunner;
		private static int s_nextDatabaseIndex = 1;
		public const string MongoDbDatabaseNamePrefix = "HordeServerTest_";

		public MongoInstance()
		{
			int databaseIndex;
			lock (s_lockObject)
			{
				if (s_mongoDbRunner == null)
				{
					// One-time setup per test run to avoid overhead of starting the external MongoDB process
					Startup.ConfigureMongoDbClient();
					s_mongoDbRunner = new MongoDbRunnerLocal();
					s_mongoDbRunner.Start();

					// Drop all the previous databases
					MongoClientSettings mongoSettings = MongoClientSettings.FromConnectionString(s_mongoDbRunner.GetConnectionString());
					MongoClient client = new MongoClient(mongoSettings);

					List<string> dropDatabaseNames = client.ListDatabaseNames().ToList();
					foreach (string dropDatabaseName in dropDatabaseNames)
					{
						if (dropDatabaseName.StartsWith(MongoDbDatabaseNamePrefix, StringComparison.Ordinal))
						{
							client.DropDatabase(dropDatabaseName);
						}
					}
				}
				databaseIndex = s_nextDatabaseIndex++;
			}

			DatabaseName = $"{MongoDbDatabaseNamePrefix}{databaseIndex}";
			ConnectionString = $"{s_mongoDbRunner.GetConnectionString()}/{DatabaseName}";
			Client = new MongoClient(MongoClientSettings.FromConnectionString(ConnectionString));
		}

		public void Dispose()
		{
			IMongoClient strictClient = Client.WithWriteConcern(new WriteConcern(journal: true));
			for (int i = 0; i < 5; i++)
			{
				strictClient.DropDatabase(DatabaseName);
				List<string> dbNames = strictClient.ListDatabaseNames().ToList();
				if (!dbNames.Contains(DatabaseName))
				{
					return;
				}
				Thread.Sleep(300);
			}

			throw new Exception($"Unable to drop MongoDB database {DatabaseName}");
		}
	}

	public sealed class RedisInstance : IDisposable
	{
		static readonly object s_lockObject = new object();

		const bool UseExistingRedisInstance = true;
		const int RedisPort = 6379;
		const int RedisDbNum = 15;

		private static string? s_redisConnectionString;
		private static int s_redisDbNum;
		private static RedisProcess? s_redisProcess;

		public string ConnectionString { get; }
		public int DatabaseNumber { get; }

		public RedisInstance()
		{
			lock (s_lockObject)
			{
				if (s_redisConnectionString == null)
				{
					int port = GetRedisPortInternal();
					s_redisConnectionString = $"127.0.0.1:{port},allowAdmin=true";
					s_redisDbNum = RedisDbNum;
				}

				ConnectionString = s_redisConnectionString;
				DatabaseNumber = s_redisDbNum;
			}
		}

		public void Dispose()
		{
		}

		static int GetRedisPortInternal()
		{
			if (UseExistingRedisInstance && !DatabaseRunner.IsPortAvailable(RedisPort))
			{
				Console.WriteLine("Using existing Redis instance on port {0}", RedisPort);
				return RedisPort;
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				s_redisProcess = new RedisProcess(NullLogger.Instance);
				s_redisProcess.Start("--save \"\" --appendonly no");

				return s_redisProcess.Port;
			}

			throw new Exception("Unable to connect to Redis");
		}
	}

	public class ServiceTest : IAsyncDisposable
	{
		private ServiceProvider? _serviceProvider = null;

		public IServiceProvider ServiceProvider
		{
			get
			{
				if (_serviceProvider == null)
				{
					IServiceCollection services = new ServiceCollection();
					ConfigureServices(services);

					_serviceProvider = services.BuildServiceProvider();
				}
				return _serviceProvider;
			}
		}

		public virtual async ValueTask DisposeAsync()
		{
			GC.SuppressFinalize(this);

			if (_serviceProvider != null)
			{
				await _serviceProvider.DisposeAsync();
				_serviceProvider = null;
			}
		}

		protected virtual void ConfigureSettings(ServerSettings settings)
		{
		}

		protected virtual void ConfigureServices(IServiceCollection services)
		{
		}
	}

	public class DatabaseIntegrationTest : ServiceTest
	{
		private static readonly object s_lockObject = new object();

		private MongoInstance? _mongoInstance;
		private MongoService? _mongoService;
		private readonly LoggerFactory _loggerFactory = new LoggerFactory();

		private RedisInstance? _redisInstance;
		private RedisService? _redisService;

		public DatabaseIntegrationTest()
		{
		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			base.ConfigureServices(services);

			services.AddSingleton(GetMongoServiceSingleton());
			services.AddSingleton(GetRedisServiceSingleton());
		}

		public override async ValueTask DisposeAsync()
		{
			await base.DisposeAsync();

			GC.SuppressFinalize(this);

			if (_mongoService != null)
			{
				await _mongoService.DisposeAsync();
			}
			_mongoInstance?.Dispose();

			if (_redisService != null)
			{
				await _redisService.DisposeAsync();
			}
			_redisInstance?.Dispose();

			_loggerFactory.Dispose();
		}

		public MongoService GetMongoServiceSingleton()
		{
			lock (s_lockObject)
			{
				if (_mongoService == null)
				{
					RedisService redisService = GetRedisServiceSingleton();

					_mongoInstance = new MongoInstance();

					ServerSettings ss = new ServerSettings();
					ss.DatabaseName = _mongoInstance.DatabaseName;
					ss.DatabaseConnectionString = _mongoInstance.ConnectionString;

					_mongoService = new MongoService(Options.Create(ss), redisService, OpenTelemetryTracers.Horde, _loggerFactory.CreateLogger<MongoService>(), _loggerFactory);
				}
			}
			return _mongoService;
		}

		public RedisService GetRedisServiceSingleton()
		{
			if (_redisService == null)
			{
				_redisInstance = new RedisInstance();
				_redisService = new RedisService(_redisInstance.ConnectionString, _redisInstance.DatabaseNumber, _loggerFactory.CreateLogger<RedisService>());

				IConnectionMultiplexer cm = _redisService.ConnectionPool.GetConnection();
				foreach (EndPoint endpoint in cm.GetEndPoints())
				{
					cm.GetServer(endpoint).FlushDatabase(_redisInstance.DatabaseNumber);
				}
			}
			return _redisService;
		}

		public static T Deref<T>(T? item)
		{
			Assert.IsNotNull(item);
			return item!;
		}

		public static T Deref<T>(ActionResult<T>? item) where T : class
		{
			Assert.IsNotNull(item?.Value);
			return item!.Value!;
		}
	}
}