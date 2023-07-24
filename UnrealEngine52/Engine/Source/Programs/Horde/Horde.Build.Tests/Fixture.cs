// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Configuration;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Artifacts;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Projects;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using PoolId = StringId<IPool>;
    using ProjectId = StringId<ProjectConfig>;
    using StreamId = StringId<IStream>;
    using TemplateId = StringId<ITemplateRef>;

	public class Fixture
	{
		public IJob Job1 { get; private set; } = null!;
		public IJob Job2 { get; private set; } = null!;
		public ITemplate Template { get; private set; } = null!;
		public IGraph Graph { get; private set; } = null!;
		public StreamId StreamId { get; private set; }
		public StreamConfig? StreamConfig { get; private set; }
		public TemplateId TemplateRefId1 { get; private set; }
		public TemplateId TemplateRefId2 { get; private set; }
		public IArtifact Job1Artifact { get; private set; } = null!;
		public string Job1ArtifactData { get; private set; } = null!;
		public IAgent Agent1 { get; private set; } = null!;
		public string Agent1Name { get; private set; } = null!;
		public const string PoolName = "TestingPool";

		public static async Task<Fixture> Create(ConfigService configService, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollection artifactCollection, AgentService agentService, ServerSettings serverSettings)
		{
			Fixture fixture = new Fixture();
			await fixture.Populate(configService, graphCollection, templateCollection, jobService, artifactCollection, agentService, serverSettings);

//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 112233, "leet.coder", "Did stuff", new []{"file.cpp"});
//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 1111, "swarm", "A shelved CL here", new []{"renderer.cpp"});
		
			return fixture;
		}

		private async Task Populate(ConfigService configService, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollection artifactCollection, AgentService agentService, ServerSettings serverSettings)
		{
			FixtureGraph fg = new FixtureGraph();
			fg.Id = ContentHash.Empty;
			fg.Schema = 1122;
			fg.Groups = new List<INodeGroup>();
			fg.Aggregates = new List<IAggregate>();
			fg.Labels = new List<ILabel>();

			Template = await templateCollection.GetOrAddAsync(new TemplateConfig { Name = "Test template" });
			Graph = await graphCollection.AddAsync(Template, null);

			TemplateRefId1 = new TemplateId("template1");
			TemplateRefId2 = new TemplateId("template2");

			List<TemplateRefConfig> templates = new List<TemplateRefConfig>();
			templates.Add(new TemplateRefConfig { Id = TemplateRefId1, Name = "Test Template" });
			templates.Add(new TemplateRefConfig { Id = TemplateRefId2, Name = "Test Template" });

			List<TabConfig> tabs = new List<TabConfig>();
			tabs.Add(new JobsTabConfig { Title = "foo", Templates = new List<TemplateId> { TemplateRefId1, TemplateRefId2 } });

			Dictionary<string, AgentConfig> agentTypes = new()
			{
				{ "Win64", new() { Pool = new PoolId(PoolName) } }
			};

			StreamId streamId = new StreamId("ue5-main");
			StreamConfig streamConfig = new StreamConfig { Id = streamId, Name = "//UE5/Main", Tabs = tabs, Templates = templates, AgentTypes = agentTypes };

			ProjectId projectId = new ProjectId("ue5");
			ProjectConfig projectConfig = new ProjectConfig { Id = projectId, Name = "UE5", Streams = new List<StreamConfig> { streamConfig } };

			GlobalConfig globalConfig = new GlobalConfig { Projects = new List<ProjectConfig> { projectConfig } };
			globalConfig.PostLoad(serverSettings);
			configService.Set(IoHash.Zero, globalConfig);

			StreamId = streamId;
			StreamConfig = streamConfig;

			Job1 = await jobService.CreateJobAsync(
				jobId: new JobId("5f283932841e7fdbcafb6ab5"),
				streamConfig: streamConfig,
				templateRefId: TemplateRefId1,
				templateHash: Template.Hash,
				graph: Graph,
				name: "hello1",
				change: 1000001,
				codeChange: 1000002,
				new CreateJobOptions { PreflightChange = 1001 }
			);
			Job1 = (await jobService.GetJobAsync(Job1.Id))!;

			Job2 = await jobService.CreateJobAsync(
				jobId: new JobId("5f69ea1b68423e921b035106"),
				streamConfig: streamConfig,
				templateRefId: new TemplateId("template-id-1"),
				templateHash: ContentHash.MD5("made-up-template-hash"),
				graph: fg,
				name: "hello2",
				change: 2000001,
				codeChange: 2000002,
				new CreateJobOptions()
			);
			Job2 = (await jobService.GetJobAsync(Job2.Id))!;

			Job1ArtifactData = "For The Horde!";
			using MemoryStream job1ArtifactStream = new MemoryStream(Encoding.UTF8.GetBytes(Job1ArtifactData));
			Job1Artifact = await artifactCollection.CreateArtifactAsync(Job1.Id, SubResourceId.Parse("22"), "myFile.txt",
				"text/plain", job1ArtifactStream);

			Agent1Name = "testAgent1";
			Agent1 = await agentService.CreateAgentAsync(Agent1Name, true, null);
		}

		private class FixtureGraph : IGraph
		{
			public ContentHash Id { get; set; } = ContentHash.Empty;
			public int Schema { get; set; }
			public IReadOnlyList<INodeGroup> Groups { get; set; } = null!;
			public IReadOnlyList<IAggregate> Aggregates { get; set; } = null!;
			public IReadOnlyList<ILabel> Labels { get; set; } = null!;
		}
	}
}