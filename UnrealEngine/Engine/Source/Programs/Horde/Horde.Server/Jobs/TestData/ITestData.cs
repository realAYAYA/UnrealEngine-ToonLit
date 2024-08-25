// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using MongoDB.Bson;

namespace Horde.Server.Jobs.TestData
{
	/// <summary>
	/// Test outcome
	/// </summary>
	public enum TestOutcome
	{
		/// <summary>
		/// The test was successful
		/// </summary>
		Success,
		/// <summary>
		/// The test failed
		/// </summary>
		Failure,
		/// <summary>
		/// The test was skipped
		/// </summary>
		Skipped,
		/// <summary>
		/// The test had an unspecified result
		/// </summary>
		Unspecified
	}

	/// <summary>
	/// Defines a testing environment based on platforms, configurations, targets, etc.
	/// </summary>
	public interface ITestMeta
	{
		/// <summary>
		/// The test meta id
		/// </summary>
		TestMetaId Id { get; }
		/// <summary>
		/// The name of the test platform
		/// </summary>
		IReadOnlyList<string> Platforms { get; }

		/// <summary>
		/// The configuration the test was run on
		/// </summary>
		IReadOnlyList<string> Configurations { get; }

		/// <summary>
		/// The build target, editor, server, client, etc
		/// </summary>
		IReadOnlyList<string> BuildTargets { get; }

		/// <summary>
		/// The uproject name associated with this test, note: may not be directly related to Horde project
		/// </summary>
		string ProjectName { get; }

		/// <summary>
		/// The rendering hardware interface used for the test
		/// </summary>
		string RHI { get; }

		/// <summary>
		/// The variation of the meta data, for example address sanitizing
		/// </summary>
		string Variation { get; }
	}

	/// <summary>
	/// A test that runs in a stream
	/// </summary>
	public interface ITest
	{
		/// <summary>
		/// The test id
		/// </summary>
		TestId Id { get; }

		/// <summary>
		/// The fully qualified name of the test 
		/// </summary>
		string Name { get; }

		/// <summary>
		/// The display name of the test 
		/// </summary>
		string? DisplayName { get; }

		/// <summary>
		/// The name of the associated suite if any
		/// </summary>
		string? SuiteName { get; }

		/// <summary>
		/// The meta data for the test 
		/// </summary>
		IReadOnlyList<TestMetaId> Metadata { get; }
	}

	/// <summary>
	/// A test suite that runs in a stream
	/// </summary>
	public interface ITestSuite
	{
		/// <summary>
		/// The test suite id
		/// </summary>
		TestSuiteId Id { get; }

		/// <summary>
		/// The name of the test suite
		/// </summary>
		string Name { get; }

		/// <summary>
		/// The tests that compose the suite
		/// </summary>
		IReadOnlyList<TestId> Tests { get; }

		/// <summary>
		/// The meta data for the test suite
		/// </summary>
		IReadOnlyList<TestMetaId> Metadata { get; }
	}

	/// <summary>
	/// Suite test data
	/// </summary>
	public interface ISuiteTestData
	{
		/// <summary>
		/// The test id
		/// </summary>
		TestId TestId { get; }

		/// <summary>
		/// The ourcome of the suite test
		/// </summary>
		TestOutcome Outcome { get; }

		/// <summary>
		/// How long the suite test ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// Test UID for looking up in test details
		/// </summary>
		string UID { get; }

		/// <summary>
		/// The number of warnings
		/// </summary>
		int? WarningCount { get; }

		/// <summary>
		/// The number of errors
		/// </summary>
		int? ErrorCount { get; }
	}

	/// <summary>
	/// Data ref with minimal data required for aggregate views
	/// </summary>
	public interface ITestDataRef
	{
		/// The test ref id
		TestRefId Id { get; }

		/// <summary>
		/// The associated stream
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The associated job
		/// </summary>
		JobId? JobId { get; }

		/// <summary>
		/// The associated job step
		/// </summary>
		JobStepId? StepId { get; }

		/// <summary>
		/// How long the test ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// The build changelist upon which the test ran, may not correspond to the job changelist
		/// </summary>
		int BuildChangeList { get; }

		/// <summary>
		/// The environment the test ran in
		/// </summary>
		TestMetaId Metadata { get; }

		/// <summary>
		/// The ITest in stream
		/// </summary>
		TestId? TestId { get; }

		/// <summary>
		/// The outcome of the test
		/// </summary>
		TestOutcome? Outcome { get; }

		/// <summary>
		/// The ITestSuite in stream
		/// </summary>
		TestSuiteId? SuiteId { get; }

		/// <summary>
		/// Suite tests skipped
		/// </summary>
		int? SuiteSkipCount { get; }

		/// <summary>
		/// Suite test warnings
		/// </summary>
		int? SuiteWarningCount { get; }

		/// <summary>
		/// Suite test errors
		/// </summary>
		int? SuiteErrorCount { get; }

		/// <summary>
		/// Suite test successes
		/// </summary>
		int? SuiteSuccessCount { get; }
	}

	/// <summary>
	/// Test data details
	/// </summary>
	public interface ITestDataDetails
	{
		/// The corresponding test ref		
		TestRefId Id { get; }

		/// <summary>
		/// The full details test data for this ref
		/// </summary>
		IReadOnlyList<ObjectId> TestDataIds { get; }

		/// <summary>
		/// Suite test data
		/// </summary>		
		IReadOnlyList<ISuiteTestData>? SuiteTests { get; }
	}

	/// <summary>
	/// The tests and suites running in a given stream
	/// </summary>
	public interface ITestStream
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// Test ids of tests running in the stream
		/// </summary>
		IReadOnlyList<TestId> Tests { get; }

		/// <summary>
		/// Test suite ids
		/// </summary>
		IReadOnlyList<TestSuiteId> TestSuites { get; }
	}

	/// <summary>
	/// Stores information about the results of a test
	/// </summary>
	public interface ITestData
	{
		/// <summary>
		/// Unique id of the test data
		/// </summary>
		ObjectId Id { get; }

		/// <summary>
		/// Stream that generated the test data
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The template reference id
		/// </summary>
		TemplateId TemplateRefId { get; }

		/// <summary>
		/// The job which produced the data
		/// </summary>
		JobId JobId { get; }

		/// <summary>
		/// The step that ran
		/// </summary>
		JobStepId StepId { get; }

		/// <summary>
		/// The changelist number that contained the data
		/// </summary>
		int Change { get; }

		/// <summary>
		/// Key used to identify the particular data
		/// </summary>
		string Key { get; }

		/// <summary>
		/// The data stored for this test
		/// </summary>
		BsonDocument Data { get; }
	}
}
