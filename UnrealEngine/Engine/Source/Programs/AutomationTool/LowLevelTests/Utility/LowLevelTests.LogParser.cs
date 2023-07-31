// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System.Text.RegularExpressions;

namespace LowLevelTests
{
	public class LowLevelTestsLogParser : UnrealLogParser
	{
		public class CatchTestResults
		{
			public bool Passed = false;
			public CatchAssertionResult Assertions;
			public CatchTestCaseResult TestCases;

			public CatchTestResults()
			{
				Assertions = new CatchAssertionResult();
				TestCases = new CatchTestCaseResult();
			}
		}

		public class CatchTestCaseResult
		{
			public string TotalCases;
			public string Passed;
			public string Failed;

			public CatchTestCaseResult()
			{
				TotalCases = "";
				Passed = "";
				Failed = "";
			}
		}
		public class CatchAssertionResult
		{
			public string TotalAssertions;
			public string Passed;
			public string Failed;

			public CatchAssertionResult()
			{
				TotalAssertions = "";
				Passed = "";
				Failed = "";
			}
		}

		internal class ParsePatterns
		{
			internal const string AllCatchTestPassed = @"All tests passed\s\((?<assertions>[\w\d]+)\sassertion[s]?\sin\s(?<testcases>[\w\d]+)\stest\scase[s]?.*\)";

			internal const string CatchTestCaseResults = @"test\scase[s]?\:\s+(?<totalcases>[\d]+)\s+\|\s+(?<passed>[\d]+)\s+passed\s+\|\s+(?<failed>[\d]+)\s+failed";

			internal const string CatchTestAssertionResults = @"assertion[s]?\:\s+(?<totalcases>[\d]+)\s+\|\s+(?<passed>[\d]+)\s+passed\s+\|\s+(?<failed>[\d]+)\s+failed";

			internal const string CatchThrowException = "Catch will terminate because it needed to throw an exception.";
		}

		public LowLevelTestsLogParser(string InContent)
			: base(InContent)
		{
		}

		public CatchTestResults GetCatchTestResults()
		{
			var TestResults = new CatchTestResults { };

			if (Regex.Matches(Content, ParsePatterns.CatchThrowException).Count > 0)
			{
				TestResults.Passed = false;
			}

			var AllCatchTestPassesPattern = ParsePatterns.AllCatchTestPassed;

			MatchCollection mCollection = Regex.Matches(Content, AllCatchTestPassesPattern);

			//If all the tests pass, report it and return.
			foreach (Match aMatch in mCollection)
			{
				if (aMatch.Success)
				{
					TestResults.Passed = true;
					TestResults.Assertions = new CatchAssertionResult
					{
						TotalAssertions = aMatch.Groups["assertions"].ToString()
					};
					TestResults.TestCases = new CatchTestCaseResult
					{
						TotalCases = aMatch.Groups["testcases"].ToString()
					};
					return TestResults;
				}
			}

			// If we did not find any matching test passed string from catch, check to see test results of pass/fail were logged.
			MatchCollection rCollection = Regex.Matches(Content, ParsePatterns.CatchTestCaseResults);

			foreach (Match aMatch in rCollection)
			{
				if (aMatch.Success)
				{
					TestResults.Passed = false;
					TestResults.TestCases = new CatchTestCaseResult()
					{

						TotalCases = aMatch.Groups["totalcases"].ToString(),
						Passed = aMatch.Groups["passed"].ToString(),
						Failed = aMatch.Groups["failed"].ToString()
					};
				}
			}

			MatchCollection aCollection = Regex.Matches(Content, ParsePatterns.CatchTestAssertionResults);

			foreach (Match aMatch in aCollection)
			{
				if (aMatch.Success)
				{
					TestResults.Passed = false;
					TestResults.Assertions = new CatchAssertionResult()
					{
						TotalAssertions = aMatch.Groups["totalcases"].ToString(),
						Passed = aMatch.Groups["passed"].ToString(),
						Failed = aMatch.Groups["failed"].ToString()
					};
				}
			}

			return TestResults;
		}
	}
}
