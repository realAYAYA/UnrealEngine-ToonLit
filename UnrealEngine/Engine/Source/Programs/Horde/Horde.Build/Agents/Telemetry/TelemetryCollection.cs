// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Agents.Pools;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Agents.Telemetry
{
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Collection of utilization data
	/// </summary>
	public class TelemetryCollection : ITelemetryCollection
	{
		class UtilizationDocument : IUtilizationTelemetry
		{
			public DateTime StartTime { get; set; }
			public DateTime FinishTime { get; set; }

			public int NumAgents { get; set; }
			public List<PoolUtilizationDocument> Pools { get; set; }
			IReadOnlyList<IPoolUtilizationTelemetry> IUtilizationTelemetry.Pools => Pools;

			public double AdminTime { get; set; }
			public double HibernatingTime { get; set; }
			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private UtilizationDocument()
			{
				Pools = new List<PoolUtilizationDocument>();
			}

			public UtilizationDocument(IUtilizationTelemetry other)
			{
				StartTime = other.StartTime;
				FinishTime = other.FinishTime;
				NumAgents = other.NumAgents;
				Pools = other.Pools.ConvertAll(x => new PoolUtilizationDocument(x));
				HibernatingTime = other.HibernatingTime;
				AdminTime = other.AdminTime;
			}
		}

		class PoolUtilizationDocument : IPoolUtilizationTelemetry
		{
			public PoolId PoolId { get; set; }
			public int NumAgents { get; set; }

			public List<StreamUtilizationDocument> Streams { get; set; } 
			IReadOnlyList<IStreamUtilizationTelemetry> IPoolUtilizationTelemetry.Streams => Streams;

			public double AdminTime { get; set; }
			public double HibernatingTime { get; set; }
			public double OtherTime { get; set; }

			[BsonConstructor]
			private PoolUtilizationDocument()
			{
				Streams = new List<StreamUtilizationDocument>();
			}

			public PoolUtilizationDocument(IPoolUtilizationTelemetry other)
			{
				PoolId = other.PoolId;
				NumAgents = other.NumAgents;
				Streams = other.Streams.ConvertAll(x => new StreamUtilizationDocument(x));
				AdminTime = other.AdminTime;
				HibernatingTime = other.HibernatingTime;
				OtherTime = other.OtherTime;
			}
		}

		class StreamUtilizationDocument : IStreamUtilizationTelemetry
		{
			public StreamId StreamId { get; set; }
			public double Time { get; set; }

			[BsonConstructor]
			private StreamUtilizationDocument()
			{
			}

			public StreamUtilizationDocument(IStreamUtilizationTelemetry other)
			{
				StreamId = other.StreamId;
				Time = other.Time;
			}
		}

		readonly IMongoCollection<UtilizationDocument> _utilization;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="database"></param>
		public TelemetryCollection(MongoService database)
		{
			_utilization = database.GetCollection<UtilizationDocument>("Utilization", keys => keys.Ascending(x => x.FinishTime).Ascending(x => x.StartTime));
		}

		/// <inheritdoc/>
		public async Task AddUtilizationTelemetryAsync(IUtilizationTelemetry newTelemetry)
		{
			await _utilization.InsertOneAsync(new UtilizationDocument(newTelemetry));
		}

		/// <inheritdoc/>
		public async Task<List<IUtilizationTelemetry>> GetUtilizationTelemetryAsync(DateTime startTimeUtc, DateTime finishTimeUtc)
		{
			FilterDefinition<UtilizationDocument> filter = Builders<UtilizationDocument>.Filter.Gte(x => x.FinishTime, startTimeUtc) & Builders<UtilizationDocument>.Filter.Lte(x => x.StartTime, finishTimeUtc);

			List<UtilizationDocument> documents = await _utilization.Find(filter).ToListAsync();
			return documents.ConvertAll<IUtilizationTelemetry>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IUtilizationTelemetry?> GetLatestUtilizationTelemetryAsync()
		{
			return await _utilization.Find(FilterDefinition<UtilizationDocument>.Empty).SortByDescending(x => x.FinishTime).FirstOrDefaultAsync();
		}
	}
}
