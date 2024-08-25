// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.TestData;
using Horde.Server.Logs;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using Moq;

namespace Horde.Server.Tests.Jobs.TestData
{
	/// <summary>
	/// Tests for the device service
	/// </summary>
	[TestClass]
	public class TestDataTests : TestSetup
	{
		const string MainStreamName = "//UE5/Main";
		readonly StreamId _mainStreamId = new StreamId(StringId.Sanitize(MainStreamName));

		const string ReleaseStreamName = "//UE5/Release";
		readonly StreamId _releaseStreamId = new StreamId(StringId.Sanitize(ReleaseStreamName));

		readonly IGraph _graph;

		TestDataController? _testDataController;

		// override DeviceController with valid user
		private new TestDataController TestDataController
		{
			get
			{
				if (_testDataController == null)
				{
					IUser user = UserCollection.FindOrAddUserByLoginAsync("TestUser").Result;
					_testDataController = base.TestDataController;
					ControllerContext controllerContext = new ControllerContext();
					controllerContext.HttpContext = new DefaultHttpContext();
					controllerContext.HttpContext.User = new ClaimsPrincipal(new ClaimsIdentity(
						new List<Claim> { HordeClaims.AdminClaim.ToClaim(),
						new Claim(ClaimTypes.Name, "TestUser"),
						new Claim(HordeClaimTypes.UserId, user.Id.ToString()) }
						, "TestAuthType"));
					_testDataController.ControllerContext = controllerContext;

				}
				return _testDataController;
			}
		}

		public static INode MockNode(string name, IReadOnlyNodeAnnotations annotations)
		{
			Mock<INode> node = new Mock<INode>(MockBehavior.Strict);
			node.SetupGet(x => x.Name).Returns(name);
			node.SetupGet(x => x.Annotations).Returns(annotations);
			return node.Object;
		}

		static StreamConfig CreateStream(StreamId streamId, string streamName)
		{
			StreamConfig streamConfig = new StreamConfig { Id = streamId, Name = streamName };
			streamConfig.Tabs.Add(new TabConfig { Title = "General", Templates = new List<TemplateId> { new TemplateId("test-template") } });
			streamConfig.Templates.Add(new TemplateRefConfig { Id = new TemplateId("test-template") });
			return streamConfig;
		}

		public TestDataTests()
		{
			ProjectConfig projectConfig = new ProjectConfig { Id = new ProjectId("ue5"), Name = "UE5" };
			projectConfig.Streams.Add(CreateStream(_mainStreamId, MainStreamName));
			projectConfig.Streams.Add(CreateStream(_releaseStreamId, ReleaseStreamName));

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Projects.Add(projectConfig);

			SetConfig(globalConfig);

			List<INode> nodes = new List<INode>();
			nodes.Add(MockNode("Update Version Files", NodeAnnotations.Empty));
			nodes.Add(MockNode("Compile UnrealHeaderTool Win64", NodeAnnotations.Empty));
			nodes.Add(MockNode("Compile ShooterGameEditor Win64", NodeAnnotations.Empty));
			nodes.Add(MockNode("Cook ShooterGame Win64", NodeAnnotations.Empty));

			Mock<INodeGroup> grp = new Mock<INodeGroup>(MockBehavior.Strict);
			grp.SetupGet(x => x.Nodes).Returns(nodes);

			Mock<IGraph> graphMock = new Mock<IGraph>(MockBehavior.Strict);
			graphMock.SetupGet(x => x.Groups).Returns(new List<INodeGroup> { grp.Object });
			_graph = graphMock.Object;
		}

		public IJob CreateJob(StreamId streamId, int change, string name, IGraph graph, TimeSpan time = default)
		{
			JobId jobId = JobIdUtils.GenerateNewId();

			List<IJobStepBatch> batches = new List<IJobStepBatch>();
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup @group = graph.Groups[groupIdx];

				List<IJobStep> steps = new List<IJobStep>();
				for (int nodeIdx = 0; nodeIdx < @group.Nodes.Count; nodeIdx++)
				{
					JobStepId stepId = new JobStepId((ushort)((groupIdx * 100) + nodeIdx));

					ILogFile logFile = LogFileService.CreateLogFileAsync(jobId, null, null, LogType.Json).Result;

					Mock<IJobStep> step = new Mock<IJobStep>(MockBehavior.Strict);
					step.SetupGet(x => x.Id).Returns(stepId);
					step.SetupGet(x => x.NodeIdx).Returns(nodeIdx);
					step.SetupGet(x => x.LogId).Returns(logFile.Id);
					step.SetupGet(x => x.StartTimeUtc).Returns(DateTime.UtcNow + time);

					steps.Add(step.Object);
				}

				JobStepBatchId batchId = new JobStepBatchId((ushort)(groupIdx * 100));

				Mock<IJobStepBatch> batch = new Mock<IJobStepBatch>(MockBehavior.Strict);
				batch.SetupGet(x => x.Id).Returns(batchId);
				batch.SetupGet(x => x.GroupIdx).Returns(groupIdx);
				batch.SetupGet(x => x.Steps).Returns(steps);
				batches.Add(batch.Object);
			}

			Mock<IJob> job = new Mock<IJob>(MockBehavior.Strict);
			job.SetupGet(x => x.Id).Returns(jobId);
			job.SetupGet(x => x.Name).Returns(name);
			job.SetupGet(x => x.StreamId).Returns(streamId);
			job.SetupGet(x => x.TemplateId).Returns(new TemplateId("test-template"));
			job.SetupGet(x => x.Change).Returns(change);
			job.SetupGet(x => x.PreflightChange).Returns(0);
			job.SetupGet(x => x.Batches).Returns(batches);
			job.SetupGet(x => x.ShowUgsBadges).Returns(false);
			job.SetupGet(x => x.ShowUgsAlerts).Returns(false);
			job.SetupGet(x => x.PromoteIssuesByDefault).Returns(false);
			job.SetupGet(x => x.UpdateIssues).Returns(false);
			job.SetupGet(x => x.NotificationChannel).Returns("#devtools-horde-slack-testing");
			return job.Object;
		}

		[TestMethod]
		public async Task SimpleReportTestAsync()
		{
			await TestDataService.StartAsync(CancellationToken.None);
			string[] streamIds = new string[] { _mainStreamId.ToString(), _releaseStreamId.ToString() };

			IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
			IJobStep step = job.Batches[0].Steps[0];
			IJob job2 = CreateJob(_releaseStreamId, 105, "Test Build", _graph);
			IJobStep step2 = job2.Batches[0].Steps[0];
			IJob job3 = CreateJob(_releaseStreamId, 105, "Test Build", _graph);
			IJobStep step3 = job3.Batches[0].Steps[0];

			BsonDocument testData1 = BsonDocument.Parse(String.Join('\n', _simpleTestDataLines));

			List<(string key, BsonDocument)> data1 = new List<(string key, BsonDocument)>();
			BsonArray items = testData1.GetValue("Items").AsBsonArray;
			foreach (BsonValue item in items.ToList())
			{
				BsonDocument value = item.AsBsonDocument.GetValue("Data").AsBsonDocument;
				data1.Add(("Simple Report Key", value));
			}

			BsonDocument testData2 = BsonDocument.Parse(String.Join('\n', _simpleTestDataLines2));

			List<(string key, BsonDocument)> data2 = new List<(string key, BsonDocument)>();
			items = testData2.GetValue("Items").AsBsonArray;
			foreach (BsonValue item in items.ToList())
			{
				BsonDocument value = item.AsBsonDocument.GetValue("Data").AsBsonDocument;
				data2.Add(("Simple Report Key", value));
			}

			await TestDataCollection.AddAsync(job, step, data1.ToArray());
			await TestDataCollection.AddAsync(job2, step2, data1.ToArray());
			await TestDataCollection.AddAsync(job3, step3, data2.ToArray());

			await TestDataService.TickForTestingAsync();

			ActionResult<List<GetTestMetaResponse>> metaResult = await TestDataController.GetTestMetaAsync();
			Assert.IsNotNull(metaResult);
			Assert.IsNotNull(metaResult.Value);
			List<GetTestMetaResponse> meta = metaResult.Value;
			Assert.AreEqual(2, meta.Count);

			ActionResult<List<GetTestStreamResponse>> streamResult = await TestDataController.GetTestStreamsAsync(streamIds);
			Assert.IsNotNull(streamResult);
			Assert.IsNotNull(streamResult.Value);
			List<GetTestStreamResponse> streams = streamResult.Value;

			Assert.AreEqual(2, streams.Count);
			Assert.AreEqual(1, streams[0].Tests.Count);
			Assert.AreEqual(2, streams[0].TestMetadata.Count);
			Assert.AreEqual(0, streams[0].TestSuites.Count);
			Assert.AreEqual(1, streams[1].Tests.Count);
			Assert.AreEqual(2, streams[1].TestMetadata.Count);
			Assert.AreEqual(0, streams[1].TestSuites.Count);

			Assert.AreEqual(streams[0].TestMetadata[0].Id, streams[1].TestMetadata[0].Id);

			ActionResult<List<GetTestDataRefResponse>> refResult = await TestDataController.GetTestDataRefAsync(streamIds, meta.Select(x => x.Id).ToArray(), streams[0].Tests.Select(t => t.Id).ToArray());
			Assert.IsNotNull(refResult);
			Assert.IsNotNull(refResult.Value);
			List<GetTestDataRefResponse> refs = refResult.Value;

			Assert.AreEqual(3, refs.Count);

			GetTestsRequest request = new GetTestsRequest() { TestIds = streams[0].Tests.Select(x => x.Id.ToString()).ToList() };
			ActionResult<List<GetTestResponse>> testResults = await TestDataController.GetTestsAsync(request);
			Assert.IsNotNull(testResults);
			Assert.IsNotNull(testResults.Value);
			Assert.IsNotNull(testResults.Value[0].Id, streams[0].Tests[0].Id);
		}

		internal class TestRequest
		{
			public List<string>? TestIds { get; set; }
		}

		[TestMethod]
		public async Task SessionReportTestAsync()
		{
			await TestDataService.StartAsync(CancellationToken.None);

			string[] streamIds = new string[] { _mainStreamId.ToString(), _releaseStreamId.ToString() };

			IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
			IJobStep step = job.Batches[0].Steps[0];
			IJob job2 = CreateJob(_releaseStreamId, 105, "Test Build", _graph);
			IJobStep step2 = job2.Batches[0].Steps[0];
			IJob job3 = CreateJob(_releaseStreamId, 105, "Test Build", _graph);
			IJobStep step3 = job3.Batches[0].Steps[0];

			BsonDocument testData = BsonDocument.Parse(String.Join('\n', _testSessionDataLines));

			List<(string key, BsonDocument)> data = new List<(string key, BsonDocument)>();
			BsonArray items = testData.GetValue("Items").AsBsonArray;
			foreach (BsonValue item in items.ToList())
			{
				BsonDocument value = item.AsBsonDocument.GetValue("Data").AsBsonDocument;
				data.Add(($"Session Report Key {data.Count}", value));
			}

			List<string> dataLines2 = new List<string>();
			for (int i = 0; i < _testSessionDataLines.Length; i++)
			{
				dataLines2.Add(_testSessionDataLines[i].Replace("Win64", "Android", StringComparison.OrdinalIgnoreCase));
			}

			BsonDocument testData2 = BsonDocument.Parse(String.Join('\n', dataLines2));

			List<(string key, BsonDocument)> data2 = new List<(string key, BsonDocument)>();
			items = testData2.GetValue("Items").AsBsonArray;
			foreach (BsonValue item in items.ToList())
			{
				BsonDocument value = item.AsBsonDocument.GetValue("Data").AsBsonDocument;
				data2.Add(($"Session Report Key {data2.Count}", value));
			}

			await TestDataCollection.AddAsync(job, step, data.ToArray());
			await TestDataCollection.AddAsync(job2, step2, data.ToArray());
			await TestDataCollection.AddAsync(job3, step3, data2.ToArray());

			await TestDataService.TickForTestingAsync();

			ActionResult<List<GetTestMetaResponse>> metaResult = await TestDataController.GetTestMetaAsync();
			Assert.IsNotNull(metaResult);
			Assert.IsNotNull(metaResult.Value);
			List<GetTestMetaResponse> meta = metaResult.Value;
			Assert.AreEqual(2, meta.Count);
			Assert.AreEqual(1, meta[0].BuildTargets.Count);
			Assert.AreEqual("Client", meta[0].BuildTargets[0]);
			Assert.AreEqual(1, meta[0].Platforms.Count);
			Assert.AreEqual("Win64", meta[0].Platforms[0]);
			Assert.AreEqual("Android", meta[1].Platforms[0]);
			Assert.AreEqual(1, meta[0].Configurations.Count);
			Assert.AreEqual("Development", meta[0].Configurations[0]);
			Assert.AreEqual("EngineTest", meta[0].ProjectName);
			Assert.AreEqual("default", meta[0].RHI);
			Assert.AreEqual("default", meta[0].Variation);

			ActionResult<List<GetTestStreamResponse>> streamResult = await TestDataController.GetTestStreamsAsync(streamIds);
			Assert.IsNotNull(streamResult);
			Assert.IsNotNull(streamResult.Value);
			List<GetTestStreamResponse> streams = streamResult.Value;

			Assert.AreEqual(2, streams.Count);

			Assert.AreEqual(0, streams[0].Tests.Count);
			Assert.AreEqual(2, streams[0].TestMetadata.Count);
			Assert.AreEqual(1, streams[0].TestSuites.Count);

			Assert.AreEqual(0, streams[1].Tests.Count);
			Assert.AreEqual(2, streams[1].TestMetadata.Count);
			Assert.AreEqual(1, streams[1].TestSuites.Count);

			//Assert.AreEqual(streams[0].TestMetadata[0].Id, streams[1].TestMetadata[0].Id);

			ActionResult<List<GetTestDataRefResponse>> refResult = await TestDataController.GetTestDataRefAsync(streamIds, meta.Select(x => x.Id).ToArray());
			Assert.IsNotNull(refResult);
			Assert.IsNotNull(refResult.Value);
			List<GetTestDataRefResponse> refs = refResult.Value;

			Assert.AreEqual(3, refs.Count);

			Assert.AreEqual(1, refs[0].SuiteSkipCount);
			Assert.AreEqual(1, refs[0].SuiteWarningCount);
			Assert.AreEqual(1, refs[0].SuiteErrorCount);
		}

		private readonly string[] _simpleTestDataLines =
		{
			@"{",
			@"  ""Items"": [",
			@"    {",
			@"      ""Key"": ""Simple Report::UE.BootTest EngineTest Editor Win64"",",
			@"      ""Data"": {",
			@"  	  ""Version"" : 1,",
			@"        ""Type"": ""Simple Report"",",
			@"        ""TestName"": ""EditorBootTest"",",
			@"        ""Description"": ""Win64 Development EditorGame"",",
			@"        ""ReportCreatedOn"": ""10/31/2022 11:44:56 AM"",",
			@"        ""TotalDurationSeconds"": 35.33694,",
			@"        ""HasSucceeded"": true,",
			@"        ""Status"": ""Passed"",",
			@"        ""URLLink"": """",",
			@"        ""BuildChangeList"": 22815797,",
			@"        ""MainRole"": {",
			@"          ""Type"": ""Editor"",",
			@"          ""Platform"": ""Win64"",",
			@"          ""Configuration"": ""Development""",
			@"        },",
			@"        ""Roles"": [",
			@"          {",
			@"            ""Type"": ""Editor"",",
			@"            ""Platform"": ""Win64"",",
			@"            ""Configuration"": ""Development""",
			@"          }",
			@"        ],",
			@"        ""TestResult"": ""Passed"",",
			@"        ""Logs"": [],",
			@"        ""Errors"": [],",
			@"        ""Warnings"": [],",
			@"        ""Metadata"": {",
			@"          ""Platform"": ""Win64"",",
			@"          ""BuildTarget"": ""Editor"",",
			@"          ""Configuration"": ""Development"",",
			@"          ""Project"": ""EngineTest""",
			@"        }",
			@"      }",
			@"    }",
			@"  ],",
			@"}"
		};

		private readonly string[] _simpleTestDataLines2 =
		{
			@"{",
			@"  ""Items"": [",
			@"    {",
			@"      ""Key"": ""Simple Report::UE.BootTest EngineTest Editor Win64"",",
			@"      ""Data"": {",
			@"  	  ""Version"" : 1,",
			@"        ""Type"": ""Simple Report"",",
			@"        ""TestName"": ""EditorBootTest"",",
			@"        ""Description"": ""Win64 Development EditorGame"",",
			@"        ""ReportCreatedOn"": ""10/31/2022 11:44:56 AM"",",
			@"        ""TotalDurationSeconds"": 35.33694,",
			@"        ""HasSucceeded"": true,",
			@"        ""Status"": ""Passed"",",
			@"        ""URLLink"": """",",
			@"        ""BuildChangeList"": 22815797,",
			@"        ""MainRole"": {",
			@"          ""Type"": ""Client"",",
			@"          ""Platform"": ""Android"",",
			@"          ""Configuration"": ""Development""",
			@"        },",
			@"        ""Roles"": [",
			@"          {",
			@"            ""Type"": ""Client"",",
			@"            ""Platform"": ""Android"",",
			@"            ""Configuration"": ""Development""",
			@"          }",
			@"        ],",
			@"        ""TestResult"": ""Passed"",",
			@"        ""Logs"": [],",
			@"        ""Errors"": [],",
			@"        ""Warnings"": [],",
			@"        ""Metadata"": {",
			@"          ""Platform"": ""Android"",",
			@"          ""BuildTarget"": ""Client"",",
			@"          ""Configuration"": ""Development"",",
			@"          ""Project"": ""EngineTest""",
			@"        }",
			@"      }",
			@"    }",
			@"  ],",
			@"}"
		};

		private readonly string[] _testSessionDataLines =
		{
			@"{",
			@"    ""Items"": [	",
			@"        {",
			@"            ""Key"": ""Automated Test Session"",",
			@"            ""Data"": {",
			@"  	          ""Version"" : 1,",
			@"                ""Type"": ""Automated Test Session"",",
			@"                ""Name"": ""UE.Automation(Group:UI) EngineTest"",",
			@"                ""PreFlightChange"": """",",
			@"                ""TestSessionInfo"": {",
			@"                    ""DateTime"": ""2022.10.31-01.35.07"",",
			@"                    ""TimeElapseSec"": 17.63286,",
			@"                    ""Tests"": {",
			@"                        ""cbdb55ea"": {",
			@"                            ""Name"": ""Project.Functional Tests.Tests.UI.Clipping.WidgetClipping.Simple UI"",",
			@"                            ""TestUID"": ""cbdb55ea"",",
			@"                            ""Suite"": ""UI"",",
			@"                            ""State"": ""Skipped"",",
			@"                            ""DeviceAppInstanceName"": [",
			@"                                ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042""",
			@"                            ],",
			@"                            ""ErrorCount"": 0,",
			@"                            ""WarningCount"": 0,",
			@"                            ""ErrorHashAggregate"": """",",
			@"                            ""DateTime"": ""2022.10.31-01.34.41"",",
			@"                            ""TimeElapseSec"": 0",
			@"                        },",
			@"                        ""eafa1362"": {",
			@"                            ""Name"": ""Project.Functional Tests.Tests.UI.Effects.UI_Effects.Blur"",",
			@"                            ""TestUID"": ""eafa1362"",",
			@"                            ""Suite"": ""UI"",",
			@"                            ""State"": ""Success"",",
			@"                            ""DeviceAppInstanceName"": [",
			@"                                ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042""",
			@"                            ],",
			@"                            ""ErrorCount"": 0,",
			@"                            ""WarningCount"": 0,",
			@"                            ""ErrorHashAggregate"": """",",
			@"                            ""DateTime"": ""2022.10.31-01.34.49"",",
			@"                            ""TimeElapseSec"": 10.5334",
			@"                        },",
			@"                        ""1521079c"": {",
			@"                            ""Name"": ""Project.Functional Tests.Tests.UI.Fonts.FontOutlineTestUI.FontOutlineTest"",",
			@"                            ""TestUID"": ""1521079c"",",
			@"                            ""Suite"": ""UI"",",
			@"                            ""State"": ""Success"",",
			@"                            ""DeviceAppInstanceName"": [",
			@"                                ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042""",
			@"                            ],",
			@"                            ""ErrorCount"": 0,",
			@"                            ""WarningCount"": 0,",
			@"                            ""ErrorHashAggregate"": """",",
			@"                            ""DateTime"": ""2022.10.31-01.35.00"",",
			@"                            ""TimeElapseSec"": 3.53325",
			@"                        },",
			@"                        ""c2638213"": {",
			@"                            ""Name"": ""Project.Functional Tests.Tests.UI.Fonts.FontRenderingTestUI.FontRenderingTest"",",
			@"                            ""TestUID"": ""c2638213"",",
			@"                            ""Suite"": ""UI"",",
			@"                            ""State"": ""Fail"",",
			@"                            ""DeviceAppInstanceName"": [",
			@"                                ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042""",
			@"                            ],",
			@"                            ""ErrorCount"": 1,",
			@"                            ""WarningCount"": 2,",
			@"                            ""ErrorHashAggregate"": """",",
			@"                            ""DateTime"": ""2022.10.31-01.35.03"",",
			@"                            ""TimeElapseSec"": 3.56621",
			@"                        }",
			@"                    },",
			@"                    ""TestResultsTestDataUID"": ""b420bdde-c030-4add-81d2-3a8404ab3e45""",
			@"                },",
			@"                ""Devices"": [",
			@"                    {",
			@"                        ""Name"": ""RDU-WIN64-12"",",
			@"                        ""AppInstanceName"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""AppInstanceLog"": ""UI_Win64_Game/Client/ClientOutput.log"",",
			@"                        ""Metadata"": {",
			@"                            ""platform"": ""Win64"",",
			@"                            ""os_version"": ""14 3"",",
			@"                            ""model"": ""Default"",",
			@"                            ""gpu"": ""Win64 GPU"",",
			@"                            ""cpumodel"": ""Win64 CPU"",",
			@"                            ""ram_in_gb"": ""5"",",
			@"                            ""render_mode"": ""ES3_1"",",
			@"                            ""rhi"": """"",
			@"                        }",
			@"                    }",
			@"                ],",
			@"                ""IndexedErrors"": {},",
			@"                ""Metadata"": {",
			@"                    ""Platform"": ""Win64"",",
			@"                    ""BuildTarget"": ""Client"",",
			@"                    ""Configuration"": ""Development"",",
			@"                    ""Project"": ""EngineTest"",",
			@"                    ""RHI"": ""default""",
			@"                }",
			@"            }",
			@"        },",
			@"        {",
			@"            ""Key"": ""Unreal Automated Tests::UE.TargetAutomation(RunTest=UI) Win64"",",
			@"            ""Data"": {",
			@"                ""Type"": ""Unreal Automated Tests"",",
			@"   		      ""Version"" : 1,",
			@"                ""Devices"": [",
			@"                    {",
			@"                        ""DeviceName"": ""RDU-WIN64-12"",",
			@"                        ""Instance"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""Platform"": ""Win64"",",
			@"                        ""OSVersion"": ""14 3"",",
			@"                        ""Model"": ""Default"",",
			@"                        ""GPU"": ""Win64 GPU"",",
			@"                        ""CPUModel"": ""Win64 CPU"",",
			@"                        ""RAMInGB"": 5,",
			@"                        ""RenderMode"": ""ES3_1"",",
			@"                        ""RHI"": """"",
			@"                    }",
			@"                ],",
			@"                ""ReportCreatedOn"": ""2022.10.31-01.35.07"",",
			@"                ""ReportURL"": """",",
			@"                ""SucceededCount"": 3,",
			@"                ""SucceededWithWarningsCount"": 0,",
			@"                ""FailedCount"": 0,",
			@"                ""NotRunCount"": 0,",
			@"                ""InProcessCount"": 0,",
			@"                ""TotalDurationSeconds"": 17.63286,",
			@"                ""Tests"": [",
			@"                    {",
			@"                        ""TestDisplayName"": ""Simple UI"",",
			@"                        ""FullTestPath"": ""Project.Functional Tests.Tests.UI.Clipping.WidgetClipping.Simple UI"",",
			@"                        ""State"": ""Skipped"",",
			@"                        ""DeviceInstance"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""Errors"": 0,",
			@"                        ""Warnings"": 0,",
			@"                        ""ArtifactName"": ""33ec1c33-71b7-41a1-84ba-d5ee2f42af8c.json""",
			@"                    },",
			@"                    {",
			@"                        ""TestDisplayName"": ""Blur"",",
			@"                        ""FullTestPath"": ""Project.Functional Tests.Tests.UI.Effects.UI_Effects.Blur"",",
			@"                        ""State"": ""Success"",",
			@"                        ""DeviceInstance"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""Errors"": 0,",
			@"                        ""Warnings"": 0,",
			@"                        ""ArtifactName"": ""450130ac-18cb-4bee-b529-c1d8ae943a61.json""",
			@"                    },",
			@"                    {",
			@"                        ""TestDisplayName"": ""FontOutlineTest"",",
			@"                        ""FullTestPath"": ""Project.Functional Tests.Tests.UI.Fonts.FontOutlineTestUI.FontOutlineTest"",",
			@"                        ""State"": ""Success"",",
			@"                        ""DeviceInstance"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""Errors"": 0,",
			@"                        ""Warnings"": 0,",
			@"                        ""ArtifactName"": ""12752762-6332-462f-8ef3-254c7502b57a.json""",
			@"                    },",
			@"                    {",
			@"                        ""TestDisplayName"": ""FontRenderingTest"",",
			@"                        ""FullTestPath"": ""Project.Functional Tests.Tests.UI.Fonts.FontRenderingTestUI.FontRenderingTest"",",
			@"                        ""State"": ""Success"",",
			@"                        ""DeviceInstance"": ""3da3633f-c82a-5c25-9a60-1e852705b65e-C0218E40D9AC4B4FBD07326E10179042"",",
			@"                        ""Errors"": 0,",
			@"                        ""Warnings"": 0,",
			@"                        ""ArtifactName"": ""711a5bd9-26dd-470b-a3c6-98f04a522e98.json""",
			@"                    }",
			@"                ],",
			@"                ""Metadata"": {",
			@"                    ""Platform"": ""Win64"",",
			@"                    ""BuildTarget"": ""Client"",",
			@"                    ""Configuration"": ""Development"",",
			@"                    ""Project"": ""EngineTest""",
			@"                }",
			@"            }",
			@"        },",
			@"        {",
			@"            ""Key"": ""Automated Test Session Result Details::b420bdde-c030-4add-81d2-3a8404ab3e45"",",
			@"            ""Data"": {",
			@"                ""cbdb55ea"": {",
			@"                    ""Events"": [",
			@"                        {",
			@"                            ""Message"": ""Skipping test because of exclude list: Sporadic failure with animated UI elements. Jira UE-113396"",",
			@"                            ""Context"": """",",
			@"                            ""Type"": ""Info"",",
			@"                            ""Tag"": ""entry"",",
			@"                            ""Hash"": """",",
			@"                            ""DateTime"": ""0001.01.01-00.00.00"",",
			@"                            ""Artifacts"": []",
			@"                        }",
			@"                    ]",
			@"                },",
			@"                ""eafa1362"": {",
			@"                    ""Events"": [",
			@"                        {",
			@"                            ""Message"": ""LogGauntlet: GauntletHeartbeat: Idle "",",
			@"                            ""Context"": """",",
			@"                            ""Type"": ""Info"",",
			@"                            ""Tag"": ""entry"",",
			@"                            ""Hash"": """",",
			@"                            ""DateTime"": ""0001.01.01-00.00.00"",",
			@"                            ""Artifacts"": []",
			@"                        },",
			@"                        {",
			@"                            ""Message"": ""Screenshot 'Blur' was similar!  Global Difference = 0.000303, Max Local Difference = 0.005257"",",
			@"                            ""Context"": ""UI_Effects/Blur"",",
			@"                            ""Type"": ""Info"",",
			@"                            ""Tag"": ""image comparison"",",
			@"                            ""Hash"": """",",
			@"                            ""DateTime"": ""0001.01.01-00.00.00"",",
			@"                            ""Artifacts"": [",
			@"                                {",
			@"                                    ""Tag"": ""unapproved"",",
			@"                                    ""ReferencePath"": ""imageCompare/UI_Effects/Blur/Win64/ES3_1/Incoming.png""",
			@"                                }",
			@"                            ]",
			@"                        }",
			@"                    ]",
			@"                },",
			@"                ""1521079c"": {",
			@"                    ""Events"": [",
			@"                        {",
			@"                            ""Message"": ""Screenshot 'FontOutlineTest' was similar!  Global Difference = 0.000083, Max Local Difference = 0.003520"",",
			@"                            ""Context"": ""FontOutlineTestUI/FontOutlineTest"",",
			@"                            ""Type"": ""Info"",",
			@"                            ""Tag"": ""image comparison"",",
			@"                            ""Hash"": """",",
			@"                            ""DateTime"": ""0001.01.01-00.00.00"",",
			@"                            ""Artifacts"": [",
			@"                                {",
			@"                                    ""Tag"": ""unapproved"",",
			@"                                    ""ReferencePath"": ""imageCompare/FontOutlineTestUI/FontOutlineTest/Win64/ES3_1/Incoming.png""",
			@"                                }",
			@"                            ]",
			@"                        }",
			@"                    ]",
			@"                },",
			@"                ""c2638213"": {",
			@"                    ""Events"": [",
			@"                        {",
			@"                            ""Message"": ""Screenshot 'FontRenderingTest' was similar!  Global Difference = 0.000000, Max Local Difference = 0.000000"",",
			@"                            ""Context"": ""FontRenderingTestUI/FontRenderingTest"",",
			@"                            ""Type"": ""Info"",",
			@"                            ""Tag"": ""image comparison"",",
			@"                            ""Hash"": """",",
			@"                            ""DateTime"": ""0001.01.01-00.00.00"",",
			@"                            ""Artifacts"": [",
			@"                                {",
			@"                                    ""Tag"": ""unapproved"",",
			@"                                    ""ReferencePath"": ""imageCompare/FontRenderingTestUI/FontRenderingTest/Win64/ES3_1/Incoming.png""",
			@"                                }",
			@"                            ]",
			@"                        }",
			@"                    ]",
			@"                }",
			@"            }",
			@"        }",
			@"    ]",
			@"}"
		};
	}
}
