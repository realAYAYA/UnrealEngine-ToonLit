// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Streams;
using MongoDB.Bson.Serialization;

namespace Horde.Server.Jobs.TestData
{
	/// <summary>
	/// Response object describing test data to store
	/// </summary>
	public class CreateTestDataRequest
	{
		/// <summary>
		/// The job which produced the data
		/// </summary>
		[Required]
		public JobId JobId { get; set; }

		/// <summary>
		/// The step that ran
		/// </summary>
		[Required]
		public JobStepId StepId { get; set; }

		/// <summary>
		/// Key used to identify the particular data
		/// </summary>
		public string Key { get; set; } = String.Empty;

		/// <summary>
		/// The data stored for this test
		/// </summary>
		[Required]
		public Dictionary<string, object> Data { get; set; } = new Dictionary<string, object>();
	}

	/// <summary>
	/// Response object describing the created document
	/// </summary>
	public class CreateTestDataResponse
	{
		/// <summary>
		/// The id for the new document
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Id of the new document</param>
		public CreateTestDataResponse(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Response object describing test results
	/// </summary>
	public class GetTestDataResponse
	{
		/// <summary>
		/// Unique id of the test data
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Stream that generated the test data
		/// </summary>
		public string StreamId { get; set; }

		/// <summary>
		/// The template reference id
		/// </summary>
		public string TemplateRefId { get; set; }

		/// <summary>
		/// The job which produced the data
		/// </summary>
		public string JobId { get; set; }

		/// <summary>
		/// The step that ran
		/// </summary>
		public string StepId { get; set; }

		/// <summary>
		/// The changelist number that contained the data
		/// </summary>
		public int Change { get; set; }

		/// <summary>
		/// Key used to identify the particular data
		/// </summary>
		public string Key { get; set; }

		/// <summary>
		/// The data stored for this test
		/// </summary>
		public Dictionary<string, object> Data { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="testData">Test data to construct from</param>
		internal GetTestDataResponse(ITestData testData)
		{
			Id = testData.Id.ToString();
			StreamId = testData.StreamId.ToString();
			TemplateRefId = testData.TemplateRefId.ToString();
			JobId = testData.JobId.ToString();
			StepId = testData.StepId.ToString();
			Change = testData.Change;
			Key = testData.Key;
			Data = BsonSerializer.Deserialize<Dictionary<string, object>>(testData.Data);
		}
	}

	/// <summary>
	/// A test emvironment running in a stream
	/// </summary>
	public class GetTestMetaResponse
	{
		/// <summary>
		/// Meta unique id for environment 
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The platforms in the environment
		/// </summary>
		public List<string> Platforms { get; set; }

		/// <summary>
		/// The build configurations being tested
		/// </summary>
		public List<string> Configurations { get; set; }

		/// <summary>
		/// The build targets being tested
		/// </summary>
		public List<string> BuildTargets { get; set; }

		/// <summary>
		/// The test project name
		/// </summary>
		public string ProjectName { get; set; }

		/// <summary>
		/// The rendering hardware interface being used with the test
		/// </summary>
		public string RHI { get; set; }

		/// <summary>
		/// The varation of the test meta data, for example address sanitizing
		/// </summary>
		public string Variation { get; set; }

		internal GetTestMetaResponse(ITestMeta meta)
		{
			Id = meta.Id.ToString();
			Platforms = meta.Platforms.Select(p => p).ToList();
			Configurations = meta.Configurations.Select(p => p).ToList();
			BuildTargets = meta.BuildTargets.Select(p => p).ToList();
			ProjectName = meta.ProjectName;
			RHI = meta.RHI;
			Variation = meta.Variation;
		}
	}

	/// <summary>
	/// A test that runs in a stream
	/// </summary>
	public class GetTestResponse
	{
		/// <summary>
		/// The id of the test
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The name of the test 
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The name of the test 
		/// </summary>
		public string? DisplayName { get; set; }

		/// <summary>
		/// The name of the test suite
		/// </summary>
		public string? SuiteName { get; set; }

		/// <summary>
		/// The meta data the test runs on
		/// </summary>
		public List<string> Metadata { get; set; }

		internal GetTestResponse(ITest test)
		{
			Id = test.Id.ToString();
			Name = test.Name;
			DisplayName = test.DisplayName;
			SuiteName = test.SuiteName?.ToString();
			Metadata = test.Metadata.Select(x => x.ToString()).ToList();
		}
	}

	/// <summary>
	/// Get tests request
	/// </summary>
	public class GetTestsRequest
	{
		/// <summary>
		/// Test ids to get
		/// </summary>
		public List<string> TestIds { get; set; } = new List<string>();
	}

	/// <summary>
	/// A test suite that runs in a stream, contain subtests
	/// </summary>
	public class GetTestSuiteResponse
	{
		/// <summary>
		/// The id of the suite
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The name of the test suite
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The meta data the test suite runs on
		/// </summary>
		public List<string> Metadata { get; set; }

		internal GetTestSuiteResponse(ITestSuite suite)
		{
			Id = suite.Id.ToString();
			Name = suite.Name;
			Metadata = suite.Metadata.Select(x => x.ToString()).ToList();
		}
	}

	/// <summary>
	/// Response object describing test results
	/// </summary>
	public class GetTestStreamResponse
	{
		/// <summary>
		/// The stream id
		/// </summary>
		public string StreamId { get; set; }

		/// <summary>
		/// Individual tests which run in the stream
		/// </summary>
		public List<GetTestResponse> Tests { get; set; }

		/// <summary>
		/// Test suites that run in the stream
		/// </summary>
		public List<GetTestSuiteResponse> TestSuites { get; set; }

		/// <summary>
		/// Test suites that run in the stream
		/// </summary>
		public List<GetTestMetaResponse> TestMetadata { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="streamId"></param>
		/// <param name="tests"></param>
		/// <param name="suites"></param>
		/// <param name="metaData"></param>
		internal GetTestStreamResponse(StreamId streamId, List<ITest> tests, List<ITestSuite> suites, List<ITestMeta> metaData)
		{
			StreamId = streamId.ToString();

			Tests = tests.Select(t => new GetTestResponse(t)).ToList();
			TestSuites = suites.Select(t => new GetTestSuiteResponse(t)).ToList();
			TestMetadata = metaData.Select(m => new GetTestMetaResponse(m)).ToList();
		}
	}

	/// <summary>
	/// Suite test data
	/// </summary>
	public class GetSuiteTestDataResponse
	{
		/// <summary>
		/// The test id
		/// </summary>
		public string TestId { get; set; }

		/// <summary>
		/// The ourcome of the suite test
		/// </summary>
		public TestOutcome Outcome { get; set; }

		/// <summary>
		/// How long the suite test ran
		/// </summary>
		public TimeSpan Duration { get; set; }

		/// <summary>
		/// Test UID for looking up in test details
		/// </summary>
		public string UID { get; set; }

		/// <summary>
		/// The number of test warnings generated
		/// </summary>
		public int? WarningCount { get; set; }

		/// <summary>
		/// The number of test errors generated
		/// </summary>
		public int? ErrorCount { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data"></param>
		public GetSuiteTestDataResponse(ISuiteTestData data)
		{
			TestId = data.TestId.ToString();
			Outcome = data.Outcome;
			Duration = data.Duration;
			UID = data.UID;
			WarningCount = data.WarningCount;
			ErrorCount = data.ErrorCount;
		}
	}

	/// <summary>
	/// Test details
	/// </summary>
	public class GetTestDataDetailsResponse
	{
		/// <summary>
		/// The corresponding test ref
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The test documents for this ref
		/// </summary>
		public List<string> TestDataIds { get; set; }

		/// <summary>
		/// Suite test data
		/// </summary>		
		public List<GetSuiteTestDataResponse>? SuiteTests { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="details"></param>
		public GetTestDataDetailsResponse(ITestDataDetails details)
		{
			Id = details.Id.ToString();
			TestDataIds = details.TestDataIds.Select(x => x.ToString()).ToList();
			SuiteTests = details.SuiteTests?.Select(x => new GetSuiteTestDataResponse(x)).ToList();
		}
	}

	/// <summary>
	/// Data ref 
	/// </summary>
	public class GetTestDataRefResponse
	{
		/// <summary>
		/// The test ref id
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The associated stream
		/// </summary>
		public string StreamId { get; set; }

		/// <summary>
		/// The associated job id
		/// </summary>
		public string? JobId { get; set; }

		/// <summary>
		/// The associated step id
		/// </summary>
		public string? StepId { get; set; }

		/// <summary>
		/// How long the test ran
		/// </summary>
		public TimeSpan Duration { get; set; }

		/// <summary>
		/// The build changelist upon which the test ran, may not correspond to the job changelist
		/// </summary>
		public int BuildChangeList { get; set; }

		/// <summary>
		/// The platform the test ran on 
		/// </summary>
		public string MetaId { get; set; }

		/// <summary>
		/// The test id in stream
		/// </summary>
		public string? TestId { get; set; }

		/// <summary>
		/// The outcome of the test
		/// </summary>
		public TestOutcome? Outcome { get; set; }

		/// <summary>
		/// The if of the stream test suite
		/// </summary>
		public string? SuiteId { get; set; }

		/// <summary>
		/// Suite tests skipped
		/// </summary>
		public int? SuiteSkipCount { get; set; }

		/// <summary>
		/// Suite test warnings
		/// </summary>
		public int? SuiteWarningCount { get; set; }

		/// <summary>
		/// Suite test errors
		/// </summary>
		public int? SuiteErrorCount { get; set; }

		/// <summary>
		/// Suite test successes
		/// </summary>
		public int? SuiteSuccessCount { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="testData"></param>
		public GetTestDataRefResponse(ITestDataRef testData)
		{
			Id = testData.Id.ToString();
			StreamId = testData.StreamId.ToString();
			JobId = testData.JobId?.ToString();
			StepId = testData.StepId?.ToString();
			Duration = testData.Duration;
			BuildChangeList = testData.BuildChangeList;
			MetaId = testData.Metadata.ToString();
			TestId = testData.TestId?.ToString();
			if (testData.TestId != null)
			{
				Outcome = testData.Outcome;
			}
			SuiteId = testData.SuiteId?.ToString();
			SuiteSkipCount = testData.SuiteSkipCount;
			SuiteWarningCount = testData.SuiteWarningCount;
			SuiteErrorCount = testData.SuiteErrorCount;
			SuiteSuccessCount = testData.SuiteSuccessCount;
		}
	}
}
