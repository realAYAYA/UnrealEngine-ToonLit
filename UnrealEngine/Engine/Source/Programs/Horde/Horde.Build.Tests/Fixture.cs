// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Agents;
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
    using ProjectId = StringId<IProject>;
    using StreamId = StringId<IStream>;
    using TemplateRefId = StringId<TemplateRef>;

	public class Fixture
	{
		public IJob Job1 { get; private set; } = null!;
		public IJob Job2 { get; private set; } = null!;
		public ITemplate Template { get; private set; } = null!;
		public IGraph Graph { get; private set; } = null!;
		public IStream? Stream { get; private set; }
		public TemplateRefId TemplateRefId1 { get; private set; }
		public TemplateRefId TemplateRefId2 { get; private set; }
		public IArtifact Job1Artifact { get; private set; } = null!;
		public string Job1ArtifactData { get; private set; } = null!;
		public IAgent Agent1 { get; private set; } = null!;
		public string Agent1Name { get; private set; } = null!;
		public const string PoolName = "TestingPool";

		public static async Task<Fixture> Create(ConfigCollection configCollection, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollection artifactCollection, StreamService streamService, AgentService agentService)
		{
			Fixture fixture = new Fixture();
			await fixture.Populate(configCollection, graphCollection, templateCollection, jobService, artifactCollection, streamService, agentService);

//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 112233, "leet.coder", "Did stuff", new []{"file.cpp"});
//			(PerforceService as PerforceServiceStub)?.AddChange("//UE5/Main", 1111, "swarm", "A shelved CL here", new []{"renderer.cpp"});
		
			return fixture;
		}

		private async Task Populate(ConfigCollection configCollection, IGraphCollection graphCollection, ITemplateCollection templateCollection, JobService jobService, IArtifactCollection artifactCollection, StreamService streamService, AgentService agentService)
		{
			FixtureGraph fg = new FixtureGraph();
			fg.Id = ContentHash.Empty;
			fg.Schema = 1122;
			fg.Groups = new List<INodeGroup>();
			fg.Aggregates = new List<IAggregate>();
			fg.Labels = new List<ILabel>();

			Template = await templateCollection.AddAsync("Test template");
			Graph = await graphCollection.AddAsync(Template);

			TemplateRefId1 = new TemplateRefId("template1");
			TemplateRefId2 = new TemplateRefId("template2");

			List<TemplateRefConfig> templates = new List<TemplateRefConfig>();
			templates.Add(new TemplateRefConfig { Id = TemplateRefId1, Name = "Test Template" });
			templates.Add(new TemplateRefConfig { Id = TemplateRefId2, Name = "Test Template" });

			List<CreateStreamTabRequest> tabs = new List<CreateStreamTabRequest>();
			tabs.Add(new CreateJobsTabRequest { Title = "foo", Templates = new List<TemplateRefId> { TemplateRefId1, TemplateRefId2 } });

			Dictionary<string, CreateAgentTypeRequest> agentTypes = new()
			{
				{ "Win64", new() { Pool = PoolName} }
			};

			StreamConfig config = new StreamConfig { Name = "//UE5/Main", Tabs = tabs, Templates = templates, AgentTypes = agentTypes };
			await configCollection.AddConfigAsync("rev1", config);

			Stream = await streamService.StreamCollection.GetAsync(new StreamId("ue5-main"));
			Stream = await streamService.StreamCollection.TryCreateOrReplaceAsync(
				new StreamId("ue5-main"),
				Stream,
				"rev1",
				new ProjectId("does-not-exist")
			);
			
			Job1 = await jobService.CreateJobAsync(
				jobId: new JobId("5f283932841e7fdbcafb6ab5"),
				stream: Stream!,
				templateRefId: TemplateRefId1,
				templateHash: Template.Id,
				graph: Graph,
				name: "hello1",
				change: 1000001,
				codeChange: 1000002,
				preflightChange: 1001,
				clonedPreflightChange: null,
				preflightDescription: null,
				startedByUserId: null,
				priority: Priority.Normal,
				null,
				null,
				null,
				null,
				false,
				false,
				null,
				null,
				arguments: new List<string>()
			);
			Job1 = (await jobService.GetJobAsync(Job1.Id))!;

			Job2 = await jobService.CreateJobAsync(
				jobId: new JobId("5f69ea1b68423e921b035106"),
				stream: Stream!,
				templateRefId: new TemplateRefId("template-id-1"),
				templateHash: ContentHash.MD5("made-up-template-hash"),
				graph: fg,
				name: "hello2",
				change: 2000001,
				codeChange: 2000002,
				preflightChange: null,
				clonedPreflightChange: null,
				preflightDescription: null,
				startedByUserId: null,
				priority: Priority.Normal,
				null,
				null,
				null,
				null,
				false,
				false,
				null,
				null,
				arguments: new List<string>()
			);
			Job2 = (await jobService.GetJobAsync(Job2.Id))!;

			Job1ArtifactData = "For The Horde!";
			using MemoryStream job1ArtifactStream = new MemoryStream(Encoding.UTF8.GetBytes(Job1ArtifactData));
			Job1Artifact = await artifactCollection.CreateArtifactAsync(Job1.Id, SubResourceId.Parse("22"), "myFile.txt",
				"text/plain", job1ArtifactStream);

			Agent1Name = "testAgent1";
			Agent1 = await agentService.CreateAgentAsync(Agent1Name, true, null, null);
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