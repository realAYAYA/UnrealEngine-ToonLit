// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using MongoDB.Bson;

namespace Horde.Build.Jobs.TestData
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Controller for the /api/v1/testdata endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class TestDataController : ControllerBase
	{
		/// <summary>
		/// Collection of job documents
		/// </summary>
		private readonly JobService _jobService;

		/// <summary>
		/// Collection of test data documents
		/// </summary>
		private readonly ITestDataCollection _testDataCollection;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobService">The job service singleton</param>
		/// <param name="testDataCollection">Collection of test data documents</param>
		public TestDataController(JobService jobService, ITestDataCollection testDataCollection)
		{
			_jobService = jobService;
			_testDataCollection = testDataCollection;
		}

		/// <summary>
		/// Creates a new TestData document
		/// </summary>
		/// <returns>The stream document</returns>
		[HttpPost]
		[Route("/api/v1/testdata")]
		public async Task<ActionResult<CreateTestDataResponse>> CreateAsync(CreateTestDataRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(new JobId(request.JobId));
			if (job == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.UpdateJob, User, null))
			{
				return Forbid();
			}

			IJobStep? jobStep;
			if (!job.TryGetStep(request.StepId.ToSubResourceId(), out jobStep))
			{
				return NotFound();
			}

			ITestData testData = await _testDataCollection.AddAsync(job, jobStep, request.Key, new BsonDocument(request.Data));
			return new CreateTestDataResponse(testData.Id.ToString());
		}

		/// <summary>
		/// Searches for test data that matches a set of criteria
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="minChange">The minimum changelist number to return (inclusive)</param>
		/// <param name="maxChange">The maximum changelist number to return (inclusive)</param>
		/// <param name="jobId">The job id</param>
		/// <param name="jobStepId">The unique step id</param>
		/// <param name="key">Key identifying the result to return</param>
		/// <param name="index">Offset within the results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <returns>The stream document</returns>
		[HttpGet]
		[Route("/api/v1/testdata")]
		[ProducesResponseType(typeof(List<GetTestDataResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindTestDataAsync([FromQuery] string? streamId = null, [FromQuery] int? minChange = null, [FromQuery] int? maxChange = null, string? jobId = null, string? jobStepId = null, string? key = null, int index = 0, int count = 10, PropertyFilter? filter = null)
		{
			StreamId? streamIdValue = null;
			if(streamId != null)
			{
				streamIdValue = new StreamId(streamId);
			}

			JobPermissionsCache cache = new JobPermissionsCache();

			List<object> results = new List<object>();

			List<ITestData> documents = await _testDataCollection.FindAsync(streamIdValue, minChange, maxChange, jobId?.ToObjectId<IJob>(), jobStepId?.ToSubResourceId(), key, index, count);
			foreach (ITestData document in documents)
			{
				if (await _jobService.AuthorizeAsync(document.JobId, AclAction.ViewJob, User, cache))
				{
					results.Add(PropertyFilter.Apply(new GetTestDataResponse(document), filter));
				}
			}

			return results;
		}

		/// <summary>
		/// Retrieve information about a specific issue
		/// </summary>
		/// <param name="testDataId">Id of the document to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/testdata/{testDataId}")]
		[ProducesResponseType(typeof(GetTestDataResponse), 200)]
		public async Task<ActionResult<object>> GetTestDataAsync(string testDataId, [FromQuery] PropertyFilter? filter = null)
		{
			ITestData? testData = await _testDataCollection.GetAsync(testDataId.ToObjectId());
			if (testData == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(testData.JobId, AclAction.ViewJob, User, null))
			{
				return Forbid();
			}

			return PropertyFilter.Apply(new GetTestDataResponse(testData), filter);
		}
	}
}
