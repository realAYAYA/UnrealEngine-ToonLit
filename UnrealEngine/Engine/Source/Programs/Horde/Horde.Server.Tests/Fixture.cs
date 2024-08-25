// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using Horde.Server.Agents;
using Horde.Server.Configuration;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Artifacts;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;

namespace Horde.Server.Tests
{
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
		public IArtifactV1 Job1Artifact { get; private set; } = null!;
		public string Job1ArtifactData { get; private set; } = null!;
		public IAgent Agent1 { get; private set; } = null!;
		public string Agent1Name { get; private set; } = null!;
		public const string PoolName = "TestingPool";

		public static async Task<Fixture> CreateAsync(ConfigService configService, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollectionV1 artifactCollection, AgentService agentService, ServerSettings serverSettings)
		{
			Fixture fixture = new Fixture();
			await fixture.PopulateAsync(configService, graphCollection, templateCollection, jobService, artifactCollection, agentService, serverSettings);

			//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 112233, "leet.coder", "Did stuff", new []{"file.cpp"});
			//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 1111, "swarm", "A shelved CL here", new []{"renderer.cpp"});

			return fixture;
		}

		private async Task PopulateAsync(ConfigService configService, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollectionV1 artifactCollection, AgentService agentService, ServerSettings serverSettings)
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
			tabs.Add(new TabConfig { Title = "foo", Templates = new List<TemplateId> { TemplateRefId1, TemplateRefId2 } });

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
			configService.OverrideConfig(globalConfig);

			StreamId = streamId;
			StreamConfig = streamConfig;

			Job1 = await jobService.CreateJobAsync(
				jobId: JobId.Parse("5f283932841e7fdbcafb6ab5"),
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
				jobId: JobId.Parse("5f69ea1b68423e921b035106"),
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
			Job1Artifact = await artifactCollection.CreateArtifactAsync(Job1.Id, JobStepId.Parse("22"), "myfile.txt",
				"text/plain", job1ArtifactStream);

			Agent1Name = "testAgent1";
			Agent1 = await agentService.CreateAgentAsync(Agent1Name, false, "");
		}

		private class FixtureGraph : IGraph
		{
			public ContentHash Id { get; set; } = ContentHash.Empty;
			public int Schema { get; set; }
			public IReadOnlyList<INodeGroup> Groups { get; set; } = null!;
			public IReadOnlyList<IAggregate> Aggregates { get; set; } = null!;
			public IReadOnlyList<ILabel> Labels { get; set; } = null!;
			public IReadOnlyList<IGraphArtifact> Artifacts { get; set; } = null!;
		}
	}
}