// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using StackExchange.Redis;

namespace Horde.Build.Tests
{
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

        public T Get(string name)
        {
            return CurrentValue;
        }

        public IDisposable OnChange(Action<T, string> listener)
        {
			return new Disposable();
        }

        public T CurrentValue { get; }
    }

	public sealed class MongoDbInstance : IDisposable
	{
		public string DatabaseName { get; }
		public string ConnectionString { get; }
		MongoClient Client { get; }

		private static readonly object s_lockObject = new object();
		private static MongoDbRunnerLocal? s_mongoDbRunner;
		private static int s_nextDatabaseIndex = 1;
		public const string MongoDbDatabaseNamePrefix = "HordeServerTest_";

		public MongoDbInstance()
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
			Client.DropDatabase(DatabaseName);
		}
	}

	public class ServiceTest : IDisposable
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

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			_serviceProvider?.Dispose();
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

		private MongoDbInstance? _mongoDbInstance;
		private MongoService? _mongoService;
		private readonly LoggerFactory _loggerFactory = new LoggerFactory();

		private static RedisRunner? s_redisRunner;
		private RedisService? _redisService;
        public const int RedisDbNum = 15;

		public DatabaseIntegrationTest()
		{
		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			services.AddSingleton(GetMongoServiceSingleton());
			services.AddSingleton(GetRedisServiceSingleton());
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			_mongoDbInstance?.Dispose();
			_mongoService?.Dispose();
			_redisService?.Dispose();
			_loggerFactory.Dispose();
		}

		public MongoService GetMongoServiceSingleton()
        {
			lock(s_lockObject)
			{
				if (_mongoService == null)
				{
					_mongoDbInstance = new MongoDbInstance();

					ServerSettings ss = new ServerSettings();
					ss.DatabaseName = _mongoDbInstance.DatabaseName;
					ss.DatabaseConnectionString = _mongoDbInstance.ConnectionString;

					_mongoService = new MongoService(Options.Create(ss), _loggerFactory);
				}
			}
			return _mongoService;
        }

		private static RedisRunner GetRedisRunner()
		{
			lock (s_lockObject)
			{
				if (s_redisRunner == null)
				{
					// One-time setup per test run to avoid overhead of starting the external Redis process
					s_redisRunner = new RedisRunner();
					s_redisRunner.Start();
				}
			}
			return s_redisRunner;
		}

		public RedisService GetRedisServiceSingleton()
        {
			if (_redisService == null)
			{
				RedisRunner redisRunner = GetRedisRunner();

				(string host, int port) = redisRunner.GetListenAddress();
				_redisService = new RedisService($"{host}:{port},allowAdmin=true", RedisDbNum, _loggerFactory);
				IConnectionMultiplexer cm = _redisService.ConnectionPool.GetConnection();

				foreach (EndPoint endpoint in cm.GetEndPoints())
				{
					cm.GetServer(endpoint).FlushDatabase(RedisDbNum);
				}
			}
			return _redisService;
        }
        
		public static T Deref<T>(T? item)
		{
			Assert.IsNotNull(item);
			return item!;
		}
	}
}