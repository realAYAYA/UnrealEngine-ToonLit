// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Gauntlet;

namespace Gauntlet.SelfTest
{
	
	/// <summary>
	/// Base class for log parser tests
	/// </summary>
	abstract class TestUnrealLogParserBase : BaseTestNode
	{
		protected string BaseDataPath = Path.Combine(Environment.CurrentDirectory, @"Engine\Source\Programs\AutomationTool\Gauntlet\SelfTest\TestData\LogParser");

		protected string GetFileContents(string FileName)
		{
			string FilePath = Path.Combine(BaseDataPath, FileName);

			if (File.Exists(FilePath) == false)
			{
				throw new TestException("Missing data file {0}", FilePath);
			}

			return File.ReadAllText(FilePath);
		}

		public override bool StartTest(int Pass, int NumPasses)
		{
			return true;
		}

		public override void TickTest()
		{
			MarkComplete(TestResult.Passed);
		}
	}

	
	[TestGroup("Framework")]
	class LogParserTestGauntletExitSuccess : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			HashSet<string> Platforms = new HashSet<string>();
			Platforms.Add("Win64");
			Platforms.Add(Gauntlet.Globals.Params.ParseValue("Platform", "Win64"));
			foreach (var Platform in Platforms)
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithTestSuccess" + Platform + "Client.txt"));

				int ExitCode = 2;
				Parser.GetTestExitCode(out ExitCode);

				if (ExitCode != 0)
				{
					throw new TestException("LogParser did not find succesful exit for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}


	[TestGroup("Framework")]
	class LogParserTestEnsure : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			HashSet<string> Platforms = new HashSet<string>();
			Platforms.Add("Win64");
			Platforms.Add(Gauntlet.Globals.Params.ParseValue("Platform", "Win64"));
			foreach (var Platform in Platforms)
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithEnsure" + Platform + "Client.txt"));

				var Ensures = Parser.GetEnsures();

				if (Ensures.Count() !=1)
				{
					throw new TestException("LogParser failed to find ensure for {0}", Platform);
				}

				var Ensure = Ensures.First();

				if (string.IsNullOrEmpty(Ensure.Message) || Ensure.Callstack.Length < 8)
				{
					throw new TestException("LogParser failed to find ensure details for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	[TestGroup("Framework")]
	class LogParserTestAssert : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			HashSet<string> Platforms = new HashSet<string>();
			Platforms.Add("Win64");
			Platforms.Add(Gauntlet.Globals.Params.ParseValue("Platform", "Win64"));
			foreach (var Platform in Platforms)
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithCheck" + Platform + "Client.txt"));

				UnrealLog.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length < 8 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly finds a fatal error in a logfile
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestFatalError : TestUnrealLogParserBase
	{
		private int IncompleteLineLength = "0x0000000000236999  [Unknown File]".Length;
		public override void TickTest()
		{
			HashSet<string> Platforms = new HashSet<string>();
			Platforms.Add("Win64");
			Platforms.Add("Linux");
			Platforms.Add(Gauntlet.Globals.Params.ParseValue("Platform", "Win64"));
			foreach (var Platform in Platforms)
			{
				Log.Info("Processing log: {Path}", Platform + "FatalError" + ".txt");
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents(Platform + "FatalError" + ".txt"));

				UnrealLog.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
				else
				{
					int IncompleteCallstackLineCount = FatalError.Callstack.Where(L => L.Length <= IncompleteLineLength).Count();
					if(IncompleteCallstackLineCount > 0)
					{
						string Lines = string.Join("\n", FatalError.Callstack.Where(L => L.Length <= IncompleteLineLength));
						throw new TestException("LogParser returned some incomplete callstack lines for {0}:\n{1}", Platform, Lines);
					}
				}
			}			

			MarkComplete(TestResult.Passed);
		}
	}

	[TestGroup("Framework")]
	class LogParserTestException : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			HashSet<string> Platforms = new HashSet<string>();
			Platforms.Add("Win64");
			Platforms.Add(Gauntlet.Globals.Params.ParseValue("Platform", "Win64"));
			foreach (var Platform in Platforms)
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithException" + Platform + "Client.txt"));

				UnrealLog.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the logfile correcly finds a RequestExit line in a log file
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestRequestExit : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			string Platform = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("Orion" + Platform + "ClientLogWithPerf.txt"));

			// Get warnings
			bool HadExit = Parser.HasRequestExit();

			if (HadExit == false)
			{
				throw new TestException("LogParser returned incorrect RequestExit");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly extracts channel-lines from a logfile
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestChannels: TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedLines = 761;

			string Platform = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("Orion" + Platform + "ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> Lines = Parser.GetLogChannel("OrionMemory");

			if (Lines.Count() != ExpectedLines)
			{
				throw new TestException("LogParser returned incorrect channel count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly pulls warnings from a log file
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestWarnings : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedWarnings = 21146;

			string Platform = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("Orion" + Platform + "ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> WarningLines = Parser.GetWarnings();

			if (WarningLines.Count() != ExpectedWarnings)
			{
				throw new TestException("LogParser returned incorrect warning count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestErrors : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedErrors = 20;

			string Platform = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("Orion" + Platform + "ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> ErrorLines = Parser.GetErrors();

			if (ErrorLines.Count() != ExpectedErrors)
			{
				throw new TestException("LogParser returned incorrect error count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	

	[TestGroup("Framework")]
	class LogParserPerfSummary : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			//string Platform = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			//string FilePath = Path.Combine(BaseDataPath, "Orion" + Platform + "ClientLogWithPerf.txt");

			//OrionTest.PerformanceSummary Summary = new OrionTest.PerformanceSummary(File.ReadAllText(FilePath));

			MarkComplete(TestResult.Passed);
		}

	}
}

