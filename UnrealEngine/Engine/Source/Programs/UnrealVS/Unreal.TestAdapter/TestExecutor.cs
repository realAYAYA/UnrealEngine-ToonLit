// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using Microsoft.VisualStudio.TestPlatform.ObjectModel;
using Microsoft.VisualStudio.TestPlatform.ObjectModel.Adapter;

namespace Unreal.TestAdapter
{
	[ExtensionUri("executor://UnrealTestExecutor")]
	class TestExecutor : ITestExecutor
	{
		CancellationTokenSource CancellationTokenSource;
		bool WasCancelled = false;

		//this does appear ever be called.
		//meaning that launched process will continue running
		public void Cancel()
		{
			WasCancelled = true;
			CancellationTokenSource?.Cancel();
		}

		public void RunTests(IEnumerable<TestCase> tests, IRunContext runContext, IFrameworkHandle frameworkHandle)
		{
			//early out if canceled before even started
			if (WasCancelled)
				return;

			using (CancellationTokenSource = new CancellationTokenSource())
			{
				var token = CancellationTokenSource.Token;
				Parallel.ForEach(tests, testCase =>
					{
						//if canceled skip but continue the loop to let the other processes to shutdown
						if (!token.IsCancellationRequested)
						{
							RunTestCase(testCase, frameworkHandle, token, runContext.IsBeingDebugged);
						}
					});
			}
		}

		void RunTestCase(TestCase testCase, IFrameworkHandle frameworkHandle, CancellationToken token, bool debug)
		{
			if(testCase.GetPropertyValue(TestDiscoverer.DisabledTestProperty, false))
			{
				frameworkHandle.RecordResult(new TestResult(testCase) { Outcome = TestOutcome.Skipped });
				return;
			}
			string xml = null;

			if (debug)
			{
				string tempFile = Path.Combine(Path.GetTempPath(), Path.GetTempFileName());
				int processId = frameworkHandle.LaunchProcessWithDebuggerAttached(testCase.Source, Path.GetDirectoryName(testCase.Source), "\"" + testCase.FullyQualifiedName + "\"  --reporter unreal -o " + tempFile, null);
				Process process = Process.GetProcessById(processId);
				process.WaitForExit();
				if (File.Exists(tempFile))
				{
					xml = File.ReadAllText(tempFile);
					File.Delete(tempFile);
				}
			}
			else
			{
				string tempFile = Path.Combine(Path.GetTempPath(), Path.GetTempFileName());
				using (Process process = new Process())
				{
					process.StartInfo.FileName = testCase.Source;

					//wrap in quotes because test names can have spaces
					process.StartInfo.Arguments = "\"" + testCase.FullyQualifiedName + "\"  --reporter unreal -o " + tempFile;
					process.StartInfo.UseShellExecute = false;

					if (token.IsCancellationRequested)
					{
						return;
					}

					frameworkHandle.RecordStart(testCase);
					process.Start();
					process.WaitForExit();
					if (File.Exists(tempFile))
					{
						xml = File.ReadAllText(tempFile);
						File.Delete(tempFile);
					}
				}
			}

			{
				var testResult = new TestResult(testCase) { Outcome = TestOutcome.NotFound };
				ParseResult(xml, testResult);
				frameworkHandle.RecordEnd(testCase, testResult.Outcome);
				frameworkHandle.RecordResult(testResult);
			}
		}

		void ParseResult(string xmlString, TestResult testResult)
		{
			TextReader textReader = new StringReader(xmlString);
			using (XmlReader xml = XmlReader.Create(textReader, new XmlReaderSettings() { IgnoreWhitespace = true }))
			{
				while (xml.Read())
				{
					switch (xml.NodeType)
					{
						case XmlNodeType.Element:
							if (xml.Name == "testcase" && xml.GetAttribute("name") == testResult.TestCase.FullyQualifiedName)
							{
								ParseTestCaseResult(xml, testResult);
							}
							break;
						case XmlNodeType.EndElement:
							if (xml.Name == "testrun")
							{
								return; //quit to avoid junk at the end of the output
							}
							break;
						default:
							break;
					}
				}
			}

		}

		void ParseTestCaseResult(XmlReader xml, TestResult testResult)
		{
			xml.Read();
			while (xml.NodeType != XmlNodeType.EndElement)
			{
				if (xml.Name == "failure")
				{
					string failure = xml.ReadElementContentAsString()?.Trim() ?? String.Empty;
					testResult.ErrorStackTrace = failure;
					testResult.Outcome = TestOutcome.Failed;
				}
				else if (xml.Name == "result")
				{
					string strSuccess = xml.GetAttribute("success");
					bool.TryParse(strSuccess, out bool success);
					testResult.Outcome = success ? TestOutcome.Passed : TestOutcome.Failed;

					string strDuration = xml.GetAttribute("duration");
					if (strDuration != null)
					{
						double.TryParse(strDuration, out double duraction);
						testResult.Duration = TimeSpan.FromSeconds(duraction);
					}

					xml.Read();
				}
				else
				{
					xml.Skip();
				}
			}
		}

		public void RunTests(IEnumerable<string> sources, IRunContext runContext, IFrameworkHandle frameworkHandle)
		{
			
		}
	}
}
