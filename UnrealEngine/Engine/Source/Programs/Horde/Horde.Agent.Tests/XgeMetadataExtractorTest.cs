// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using Horde.Agent.Utility;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests;

[TestClass]
public class XgeMetadataExtractorTest
{
	[TestMethod]
	public void GetSummaryFromJson()
	{
		byte[] jsonData = Encoding.UTF8.GetBytes(JsonFromIbMon);

		XgeTaskMetadataSummary summary = XgeMetadataExtractor.GetSummaryFromJson(jsonData)!;
		Assert.AreEqual("UAT_XGE", summary.Title);
		
		Assert.AreEqual(1, summary.LocalTaskCount);
		Assert.AreEqual(1, summary.RemoteTaskCount);
		
		Assert.AreEqual(TimeSpan.FromMilliseconds(2471), summary.TotalLocalTaskDuration);
		Assert.AreEqual(TimeSpan.FromMilliseconds(11706), summary.TotalRemoteTaskDuration);
	}

	private const string JsonFromIbMon = @"
{
 ""Metadata"":{
	""ID"":""{BF63E95E-F7F7-4551-8F6F-C85D62FF62DD}"",
	""Caption"":""UAT_XGE"",
	""ComputerName"":""MY-COMPUTER"",
	""UserName"":""foo.bar"",
	""ErrorCount"":0,
	""WarningCount"":0,
	""SystemErrorCount"":0,
	""SystemWarningCount"":2,
	""StartTime"":""20-01-2023 13;40;22"",
	""Duration"":""12:07:14 AM"",
	""Cancelled"":""false"",
	""ProductVersion"":2005956,
	""ProductBuildNum"":4956,
	""ProductVersionText"":""10.1.7 (build 4956)"",
	""SystemAutoRecoveryCount"":0,
	""ProjectFormat"":"""",
	""LoggingLevel"":4,
	""PrimaryFilename"":""D:\\Workspaces\\ue5-main\\Engine\\Programs\\AutomationTool\\Saved\\Logs\\UAT_XGE.xml"",
	""Incomplete"":""false"",
	""OverrideBuildStatusToSuccess"":""false""
 },
 ""Tasks"":[
  {
   ""TaskID"":44,
   ""Name"":""GPUSkinVertexFactory.ispc"",
   ""BeginTime"":1823,
   ""GUID"":""{2A2D4ECD-A1F2-4045-8D12-45C336585FCD}"",
   ""CmdLine"":"" @\""../Intermediate/Build/Win64/UnrealEditor/Development/Engine/GPUSkinVertexFactory.ispc.generated.dummy.h.response\"""",
   ""AppName"":""D:\\Workspaces\\ue5-main\\Engine\\Source\\ThirdParty\\Intel\\ISPC\\bin\\Windows\\ispc.exe"",
   ""WorkerID"":""MY-COMPUTER\\8"",
   ""Remote"":""false"",
   ""EndTime"":4294,
   ""Errors"":0,
   ""Warnings"":0,
   ""ExitCodeExists"":""true"",
   ""ExitCode"":0,
   ""Double"":""false""
  },
  {
   ""TaskID"":428,
   ""Name"":""Module.ChaosCore.cpp"",
   ""BeginTime"":6077,
   ""GUID"":""{BB171990-E012-4A48-AA7B-E38F10F7F089}"",
   ""CmdLine"":"" @..\\Intermediate\\Build\\Win64\\UnrealEditor\\Development\\ChaosCore\\Module.ChaosCore.cpp.obj.response"",
   ""AppName"":""C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.31.31103\\bin\\Hostx64\\x64\\cl.exe"",
   ""WorkerID"":""OTHER-COMPUTER\\8"",
   ""Remote"":""true"",
   ""EndTime"":17783,
   ""Errors"":0,
   ""Warnings"":0,
   ""ExitCodeExists"":""true"",
   ""ExitCode"":0,
   ""Double"":""false""
  }
 ]
}
";

}


