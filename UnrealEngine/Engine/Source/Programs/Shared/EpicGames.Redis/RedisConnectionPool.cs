// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using StackExchange.Redis;

namespace EpicGames.Redis
{
	/// <summary>
	/// A connection pool for Redis client
	///
	/// Wraps multiple ConnectionMultiplexers as lazy values and initializes them as needed.
	/// If full, the least loaded connection will be picked.
	/// </summary>
	public class RedisConnectionPool
	{
		private readonly ConcurrentBag<Lazy<ConnectionMultiplexer>> _connections;
		private readonly int _defaultDatabaseIndex;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="poolSize">Size of the pool (i.e max number of connections)</param>
		/// <param name="redisConfString">Configuration string for Redis</param>
		/// <param name="defaultDatabaseIndex">The Redis database index to use. Use -1 for the default one</param>
		public RedisConnectionPool(int poolSize, string redisConfString, int defaultDatabaseIndex = -1)
		{
			_defaultDatabaseIndex = defaultDatabaseIndex;
			_connections = new ConcurrentBag<Lazy<ConnectionMultiplexer>>();
			for (int i = 0; i < poolSize; i++)
			{
				_connections.Add(new Lazy<ConnectionMultiplexer>(() => ConnectionMultiplexer.Connect(redisConfString)));
			}
		}

		/// <summary>
		/// Get a connection from the pool
		///
		/// It will pick the least loaded connection or create a new one (if pool size allows)
		/// </summary>
		/// <returns>A Redis database connection</returns>
		public IConnectionMultiplexer GetConnection()
		{
			Lazy<ConnectionMultiplexer> lazyConnection;
			IEnumerable<Lazy<ConnectionMultiplexer>>? lazyConnections = _connections.Where(x => x.IsValueCreated);

			if (lazyConnections.Count() == _connections.Count)
			{
				// No more new connections can be created, pick the least loaded one
				lazyConnection = _connections.OrderBy(x => x.Value.GetCounters().TotalOutstanding).First();
			}
			else
			{
				// Create a new connection by picking a not yet initialized lazy value 
				lazyConnection = _connections.First(x => !x.IsValueCreated);
			}

			return lazyConnection.Value;
		}

		/// <summary>
		/// Shortcut to getting a IDatabase
		/// </summary>
		/// <returns>A Redis database</returns>
		public IDatabase GetDatabase()
		{
			return GetConnection().GetDatabase(_defaultDatabaseIndex);
		}
	}
}