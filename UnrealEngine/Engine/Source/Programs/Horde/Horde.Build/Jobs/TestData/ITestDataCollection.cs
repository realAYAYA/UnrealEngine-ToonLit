// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using MongoDB.Bson;

namespace Horde.Build.Jobs.TestData
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Collection of test data documents
	/// </summary>
	public interface ITestDataCollection
	{
		/// <summary>
		/// Creates a new test data document
		/// </summary>
		/// <param name="job">The job containing the step</param>
		/// <param name="step">The step producing the data</param>
		/// <param name="key">Key identifying the test</param>
		/// <param name="value">The data to store</param>
		/// <returns>The new stream document</returns>
		Task<ITestData> AddAsync(IJob job, IJobStep step, string key, BsonDocument value);

		/// <summary>
		/// Gets a stream by ID
		/// </summary>
		/// <param name="id">Unique id of the stream</param>
		/// <returns>The stream document</returns>
		Task<ITestData?> GetAsync(ObjectId id);

		/// <summary>
		/// Searches for test data that matches a set of criteria
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="minChange">The minimum changelist number to return (inclusive)</param>
		/// <param name="maxChange">The maximum changelist number to return (inclusive)</param>
		/// <param name="jobId">The job id</param>
		/// <param name="stepId">The unique step id</param>
		/// <param name="key">Key identifying the result to return</param>
		/// <param name="index">Offset within the results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>The stream document</returns>
		Task<List<ITestData>> FindAsync(StreamId? streamId, int? minChange, int? maxChange, JobId? jobId, SubResourceId? stepId, string? key = null, int index = 0, int count = 10);

		/// <summary>
		/// Delete the test data
		/// </summary>
		/// <param name="id">Unique id of the test data</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(ObjectId id);
	}
}
