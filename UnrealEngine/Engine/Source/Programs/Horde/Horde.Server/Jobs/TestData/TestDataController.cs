// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Streams;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using MongoDB.Bson;

namespace Horde.Server.Jobs.TestData
{
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

		readonly TestDataService _testDataService;

		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public TestDataController(TestDataService testDataService, JobService jobService, ITestDataCollection testDataCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_jobService = jobService;
			_testDataCollection = testDataCollection;
			_testDataService = testDataService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Get metadata 
		/// </summary>
		/// <param name="projects"></param>
		/// <param name="platforms"></param>
		/// <param name="targets"></param>
		/// <param name="configurations"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/testdata/metadata")]
		[ProducesResponseType(typeof(List<GetTestMetaResponse>), 200)]
		public async Task<ActionResult<List<GetTestMetaResponse>>> GetTestMetaAsync(
			[FromQuery(Name = "project")] string[]? projects = null,
			[FromQuery(Name = "platform")] string[]? platforms = null,
			[FromQuery(Name = "target")] string[]? targets = null,
			[FromQuery(Name = "configuration")] string[]? configurations = null)
		{
			IReadOnlyList<ITestMeta> metaData = await _testDataService.FindTestMetaAsync(projects, platforms, configurations, targets);
			return metaData.ConvertAll(m => new GetTestMetaResponse(m));
		}

		/// <summary>
		/// Get test details from provided refs
		/// </summary>
		/// <param name="ids"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/testdata/details")]
		[ProducesResponseType(typeof(List<GetTestDataDetailsResponse>), 200)]
		public async Task<ActionResult<List<GetTestDataDetailsResponse>>> GetTestDetailsAsync([FromQuery(Name = "id")] string[] ids)
		{
			TestRefId[] idValues = Array.ConvertAll(ids, x => TestRefId.Parse(x));
			IReadOnlyList<ITestDataDetails> details = await _testDataService.FindTestDetailsAsync(idValues);
			return details.Select(d => new GetTestDataDetailsResponse(d)).ToList();
		}

		/// <summary>
		/// Get test details from provided refs
		/// </summary>
		/// <param name="request"></param>
		/// <returns></returns>
		[HttpPost]
		[Route("/api/v2/testdata/tests")]
		[ProducesResponseType(typeof(List<GetTestResponse>), 200)]
		public async Task<ActionResult<List<GetTestResponse>>> GetTestsAsync([FromBody] GetTestsRequest request)
		{
			HashSet<string> testIds = new HashSet<string>(request.TestIds);

			IReadOnlyList<ITest> testValues = await _testDataService.FindTestsAsync(testIds.Select(x => TestId.Parse(x)).ToArray());

			return testValues.Select(x => new GetTestResponse(x)).ToList();
		}

		/// <summary>
		/// Get stream test data for the provided ids
		/// </summary>
		/// <param name="streamIds"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/testdata/streams")]
		[ProducesResponseType(typeof(List<GetTestStreamResponse>), 200)]
		public async Task<ActionResult<List<GetTestStreamResponse>>> GetTestStreamsAsync([FromQuery(Name = "Id")] string[] streamIds)
		{
			StreamId[] streamIdValues = Array.ConvertAll(streamIds, x => new StreamId(x));

			List<StreamId> queryStreams = new List<StreamId>();
			List<GetTestStreamResponse> responses = new List<GetTestStreamResponse>();

			// authorize streams
			foreach (StreamId streamId in streamIdValues)
			{
				if (_globalConfig.Value.TryGetStream(streamId, out StreamConfig? streamConfig) && streamConfig.Authorize(JobAclAction.ViewJob, User))
				{
					queryStreams.Add(streamId);
				}
			}

			if (queryStreams.Count == 0)
			{
				return responses;
			}

			HashSet<TestId> testIds = new HashSet<TestId>();
			HashSet<TestMetaId> metaIds = new HashSet<TestMetaId>();

			IReadOnlyList<ITestStream> streams = await _testDataService.FindTestStreamsAsync(queryStreams.ToArray());

			// flatten requested streams to single service queries		
			HashSet<TestSuiteId> suiteIds = new HashSet<TestSuiteId>();
			for (int i = 0; i < streams.Count; i++)
			{
				foreach (TestId testId in streams[i].Tests)
				{
					testIds.Add(testId);
				}

				foreach (TestSuiteId suiteId in streams[i].TestSuites)
				{
					suiteIds.Add(suiteId);
				}
			}

			IReadOnlyList<ITestSuite> suites = new List<ITestSuite>();
			if (suiteIds.Count > 0)
			{
				suites = await _testDataService.FindTestSuitesAsync(suiteIds.ToArray());
			}

			IReadOnlyList<ITest> tests = new List<ITest>();
			if (testIds.Count > 0)
			{
				tests = await _testDataService.FindTestsAsync(testIds.ToArray());
			}

			// gather all meta data
			IReadOnlyList<ITestMeta> metaData = new List<ITestMeta>();
			foreach (ITest test in tests)
			{
				foreach (TestMetaId metaId in test.Metadata)
				{
					metaIds.Add(metaId);
				}
			}

			foreach (ITestSuite suite in suites)
			{
				foreach (TestMetaId metaId in suite.Metadata)
				{
					metaIds.Add(metaId);
				}
			}

			if (metaIds.Count > 0)
			{
				metaData = await _testDataService.FindTestMetaAsync(metaIds: metaIds.ToArray());
			}

			// generate individual stream responses
			foreach (ITestStream s in streams)
			{
				List<ITest> streamTests = tests.Where(x => s.Tests.Contains(x.Id)).ToList();

				List<ITestSuite> streamSuites = new List<ITestSuite>();
				foreach (TestSuiteId suiteId in s.TestSuites)
				{
					ITestSuite? suite = suites.FirstOrDefault(x => x.Id == suiteId);
					if (suite != null)
					{
						streamSuites.Add(suite);
					}
				}

				HashSet<TestMetaId> streamMetaIds = new HashSet<TestMetaId>();
				foreach (ITest test in streamTests)
				{
					foreach (TestMetaId id in test.Metadata)
					{
						streamMetaIds.Add(id);
					}
				}

				foreach (ITestSuite suite in streamSuites)
				{
					foreach (TestMetaId id in suite.Metadata)
					{
						streamMetaIds.Add(id);
					}
				}

				List<ITestMeta> streamMetaData = metaData.Where(x => streamMetaIds.Contains(x.Id)).ToList();

				responses.Add(new GetTestStreamResponse(s.StreamId, streamTests, streamSuites, streamMetaData));
			}

			return responses;
		}

		/// <summary>
		/// Gets test data refs 
		/// </summary>
		/// <param name="streamIds"></param>
		/// <param name="testIds"></param>
		/// <param name="suiteIds"></param>
		/// <param name="metaIds"></param>
		/// <param name="minCreateTime"></param>
		/// <param name="maxCreateTime"></param>
		/// <param name="minChange"></param>
		/// <param name="maxChange"></param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v2/testdata/refs")]
		[ProducesResponseType(typeof(List<GetTestDataRefResponse>), 200)]
		public async Task<ActionResult<List<GetTestDataRefResponse>>> GetTestDataRefAsync(
			[FromQuery(Name = "Id")] string[] streamIds,
			[FromQuery(Name = "Mid")] string[] metaIds,
			[FromQuery(Name = "Tid")] string[]? testIds = null,
			[FromQuery(Name = "Sid")] string[]? suiteIds = null,
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] int? minChange = null,
			[FromQuery] int? maxChange = null)
		{
			StreamId[] streamIdValues = Array.ConvertAll(streamIds, x => new StreamId(x));

			List<StreamId> queryStreams = new List<StreamId>();
			List<GetTestDataRefResponse> responses = new List<GetTestDataRefResponse>();

			// authorize streams
			foreach (StreamId streamId in streamIdValues)
			{
				if (_globalConfig.Value.TryGetStream(streamId, out StreamConfig? streamConfig) && streamConfig.Authorize(JobAclAction.ViewJob, User))
				{
					queryStreams.Add(streamId);
				}
			}

			if (queryStreams.Count == 0)
			{
				return responses;
			}

			IReadOnlyList<ITestDataRef> dataRefs = await _testDataService.FindTestRefsAsync(queryStreams.ToArray(), metaIds.ConvertAll(x => TestMetaId.Parse(x)).ToArray(), testIds, suiteIds, minCreateTime?.UtcDateTime, maxCreateTime?.UtcDateTime, minChange, maxChange);
			foreach (ITestDataRef d in dataRefs)
			{
				responses.Add(new GetTestDataRefResponse(d));
			}

			return responses;
		}

		/// <summary>
		/// Creates a new TestData document
		/// </summary>
		/// <returns>The stream document</returns>
		[HttpPost]
		[Route("/api/v1/testdata")]
		public async Task<ActionResult<CreateTestDataResponse>> CreateAsync(CreateTestDataRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(request.JobId);
			if (job == null)
			{
				return NotFound();
			}
			if (!_globalConfig.Value.Authorize(job, JobAclAction.UpdateJob, User))
			{
				return Forbid();
			}

			IJobStep? jobStep;
			if (!job.TryGetStep(request.StepId, out jobStep))
			{
				return NotFound();
			}

			IReadOnlyList<ITestData> testData = await _testDataCollection.AddAsync(job, jobStep, new (string key, BsonDocument value)[] { (request.Key, new BsonDocument(request.Data)) });
			return new CreateTestDataResponse(testData[0].Id.ToString());
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The stream document</returns>
		[HttpGet]
		[Route("/api/v1/testdata")]
		[ProducesResponseType(typeof(List<GetTestDataResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindTestDataAsync([FromQuery] string? streamId = null, [FromQuery] int? minChange = null, [FromQuery] int? maxChange = null, JobId? jobId = null, JobStepId? jobStepId = null, string? key = null, int index = 0, int count = 10, PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			StreamId? streamIdValue = null;
			if (streamId != null)
			{
				streamIdValue = new StreamId(streamId);
			}

			List<object> results = new List<object>();

			IReadOnlyList<ITestData> documents = await _testDataCollection.FindAsync(streamIdValue, minChange, maxChange, jobId, jobStepId, key, index, count, cancellationToken);
			foreach (ITestData document in documents)
			{
				if (await _jobService.AuthorizeAsync(document.JobId, JobAclAction.ViewJob, User, _globalConfig.Value, cancellationToken))
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/testdata/{testDataId}")]
		[ProducesResponseType(typeof(GetTestDataResponse), 200)]
		public async Task<ActionResult<object>> GetTestDataAsync(string testDataId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			ITestData? testData = await _testDataCollection.GetAsync(ObjectId.Parse(testDataId), cancellationToken);
			if (testData == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(testData.JobId, JobAclAction.ViewJob, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			return PropertyFilter.Apply(new GetTestDataResponse(testData), filter);
		}
	}
}
