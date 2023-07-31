// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Build.Jobs.TestData
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Collection of test data documents
	/// </summary>
	public class TestDataCollection : ITestDataCollection
	{
		/// <summary>
		/// Information about a test data document
		/// </summary>
		class TestDataDocument : ITestData
		{
			public ObjectId Id { get; set; }
			public StreamId StreamId { get; set; }
			public TemplateRefId TemplateRefId { get; set; }
			public JobId JobId { get; set; }
			public SubResourceId StepId { get; set; }
			public int Change { get; set; }
			public string Key { get; set; }
			public BsonDocument Data { get; set; }

			private TestDataDocument()
			{
				Key = String.Empty;
				Data = new BsonDocument();
			}

			public TestDataDocument(IJob job, IJobStep jobStep, string key, BsonDocument value)
			{
				Id = ObjectId.GenerateNewId();
				StreamId = job.StreamId;
				TemplateRefId = job.TemplateId;
				JobId = job.Id;
				StepId = jobStep.Id;
				Change = job.Change;
				Key = key;
				Data = value;
			}
		}

		/// <summary>
		/// The stream collection
		/// </summary>
		readonly IMongoCollection<TestDataDocument> _testDataDocuments;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public TestDataCollection(MongoService mongoService)
		{
			List<MongoIndex<TestDataDocument>> indexes = new List<MongoIndex<TestDataDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.StreamId).Ascending(x => x.Change).Ascending(x => x.Key));
			indexes.Add(keys => keys.Ascending(x => x.JobId).Ascending(x => x.StepId).Ascending(x => x.Key), unique: true);
			_testDataDocuments = mongoService.GetCollection<TestDataDocument>("TestData", indexes);
		}

		/// <inheritdoc/>
		public async Task<ITestData> AddAsync(IJob job, IJobStep step, string key, BsonDocument value)
		{
			TestDataDocument newTestData = new TestDataDocument(job, step, key, value);
			await _testDataDocuments.InsertOneAsync(newTestData);
			return newTestData;
		}

		/// <inheritdoc/>
		public async Task<ITestData?> GetAsync(ObjectId id)
		{
			return await _testDataDocuments.Find<TestDataDocument>(x => x.Id == id).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<ITestData>> FindAsync(StreamId? streamId, int? minChange, int? maxChange, JobId? jobId, SubResourceId? stepId, string? key = null, int index = 0, int count = 10)
		{
			FilterDefinition<TestDataDocument> filter = FilterDefinition<TestDataDocument>.Empty;
			if (streamId != null)
			{
				filter &= Builders<TestDataDocument>.Filter.Eq(x => x.StreamId, streamId.Value);
				if (minChange != null)
				{
					filter &= Builders<TestDataDocument>.Filter.Gte(x => x.Change, minChange.Value);
				}
				if (maxChange != null)
				{
					filter &= Builders<TestDataDocument>.Filter.Lte(x => x.Change, maxChange.Value);
				}
			}
			if (jobId != null)
			{
				filter &= Builders<TestDataDocument>.Filter.Eq(x => x.JobId, jobId.Value);
				if (stepId != null)
				{
					filter &= Builders<TestDataDocument>.Filter.Eq(x => x.StepId, stepId.Value);
				}
			}
			if (key != null)
			{
				filter &= Builders<TestDataDocument>.Filter.Eq(x => x.Key, key);
			}

			SortDefinition<TestDataDocument> sort = Builders<TestDataDocument>.Sort.Ascending(x => x.StreamId).Descending(x => x.Change);

			List<TestDataDocument> results = await _testDataDocuments.Find(filter).Sort(sort).Skip(index).Limit(count).ToListAsync();
			return results.ConvertAll<ITestData>(x => x);
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(ObjectId id)
		{
			await _testDataDocuments.DeleteOneAsync<TestDataDocument>(x => x.Id == id);
		}
	}
}
