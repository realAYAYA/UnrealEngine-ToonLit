// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Execution
{
	class TestExecutor : JobExecutor
	{
		public const string Name = "Test";

		public TestExecutor(ISession session, string jobId, string batchId, string agentTypeName, IHttpClientFactory httpClientFactory, ILogger logger)
			: base(session, jobId, batchId, agentTypeName, httpClientFactory, logger)
		{
		}

		public override Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Initializing");
			return Task.CompletedTask;
		}

		protected override async Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("**** BEGIN JOB SETUP ****");

			await Task.Delay(5000, cancellationToken);

			UpdateGraphRequest updateGraph = new UpdateGraphRequest();
			updateGraph.JobId = _jobId;

			CreateGroupRequest winEditorGroup = CreateGroup("AnyAgent");
			winEditorGroup.Nodes.Add(CreateNode("Update Version Files", Array.Empty<string>(), JobStepOutcome.Success));
			winEditorGroup.Nodes.Add(CreateNode("Compile UnrealHeaderTool Win64", new string[] { "Update Version Files" }, JobStepOutcome.Success));
			winEditorGroup.Nodes.Add(CreateNode("Compile UE4Editor Win64", new string[] { "Compile UnrealHeaderTool Win64" }, JobStepOutcome.Success));
			winEditorGroup.Nodes.Add(CreateNode("Compile FortniteEditor Win64", new string[] { "Compile UnrealHeaderTool Win64", "Compile UE4Editor Win64" }, JobStepOutcome.Success));
			updateGraph.Groups.Add(winEditorGroup);

			CreateGroupRequest winToolsGroup = CreateGroup("AnyAgent"); 
			winToolsGroup.Nodes.Add(CreateNode("Compile Tools Win64", new string[] { "Compile UnrealHeaderTool Win64" }, JobStepOutcome.Warnings));
			updateGraph.Groups.Add(winToolsGroup);

			CreateGroupRequest winClientsGroup = CreateGroup("AnyAgent");
			winClientsGroup.Nodes.Add(CreateNode("Compile FortniteClient Win64", new string[] { "Compile UnrealHeaderTool Win64" }, JobStepOutcome.Success));
			updateGraph.Groups.Add(winClientsGroup);

			CreateGroupRequest winCooksGroup = CreateGroup("AnyAgent");
			winCooksGroup.Nodes.Add(CreateNode("Cook FortniteClient Win64", new string[] { "Compile FortniteEditor Win64", "Compile Tools Win64" }, JobStepOutcome.Warnings));
			winCooksGroup.Nodes.Add(CreateNode("Stage FortniteClient Win64", new string[] { "Cook FortniteClient Win64", "Compile Tools Win64" }, JobStepOutcome.Success));
			winCooksGroup.Nodes.Add(CreateNode("Publish FortniteClient Win64", new string[] { "Stage FortniteClient Win64" }, JobStepOutcome.Success));
			updateGraph.Groups.Add(winCooksGroup);

			CreateAggregateRequest aggregate = new CreateAggregateRequest();
			aggregate.Name = "Full Build";
			aggregate.Nodes.Add("Publish FortniteClient Win64");
			updateGraph.Aggregates.Add(aggregate);

			Dictionary<string, string[]> dependencyMap = CreateDependencyMap(updateGraph.Groups);
			updateGraph.Labels.Add(CreateLabel("Editors", "UE4", new string[] { "Compile UE4Editor Win64" }, Array.Empty<string>(), dependencyMap));
			updateGraph.Labels.Add(CreateLabel("Editors", "Fortnite", new string[] { "Compile FortniteEditor Win64" }, Array.Empty<string>(), dependencyMap));
			updateGraph.Labels.Add(CreateLabel("Clients", "Fortnite", new string[] { "Cook FortniteClient Win64" }, new string[] { "Publish FortniteClient Win64" }, dependencyMap));

			await RpcConnection.InvokeAsync((HordeRpc.HordeRpcClient x) => x.UpdateGraphAsync(updateGraph, null, null, cancellationToken), cancellationToken);

			logger.LogInformation("**** FINISH JOB SETUP ****");
			return true;
		}

		static CreateGroupRequest CreateGroup(string agentType)
		{
			CreateGroupRequest request = new CreateGroupRequest();
			request.AgentType = agentType;
			return request;
		}

		static CreateNodeRequest CreateNode(string name, string[] inputDependencies, JobStepOutcome outcome)
		{
			CreateNodeRequest request = new CreateNodeRequest();
			request.Name = name;
			request.InputDependencies.AddRange(inputDependencies);
			request.Properties.Add("Action", "Build");
			request.Properties.Add("Outcome", outcome.ToString());
			return request;
		}

		static CreateLabelRequest CreateLabel(string category, string name, string[] requiredNodes, string[] includedNodes, Dictionary<string, string[]> dependencyMap)
		{
			CreateLabelRequest request = new CreateLabelRequest();
			request.DashboardName = name;
			request.DashboardCategory = category;
			request.RequiredNodes.AddRange(requiredNodes);
			request.IncludedNodes.AddRange(Enumerable.Union(requiredNodes, includedNodes).SelectMany(x => dependencyMap[x]).Distinct());
			return request;
		}

		static Dictionary<string, string[]> CreateDependencyMap(IEnumerable<CreateGroupRequest> groups)
		{
			Dictionary<string, string[]> nameToDependencyNames = new Dictionary<string, string[]>();
			foreach (CreateGroupRequest group in groups)
			{
				foreach (CreateNodeRequest node in group.Nodes)
				{
					HashSet<string> dependencyNames = new HashSet<string> { node.Name };

					foreach (string inputDependency in node.InputDependencies)
					{
						dependencyNames.UnionWith(nameToDependencyNames[inputDependency]);
					}
					foreach (string orderDependency in node.OrderDependencies)
					{
						dependencyNames.UnionWith(nameToDependencyNames[orderDependency]);
					}

					nameToDependencyNames[node.Name] = dependencyNames.ToArray();
				}
			}
			return nameToDependencyNames;
		}

		protected override async Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("**** BEGIN NODE {StepName} ****", step.Name);

			await Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken);
			cancellationToken.ThrowIfCancellationRequested();

			JobStepOutcome outcome = Enum.Parse<JobStepOutcome>(step.Properties["Outcome"]);

			Dictionary<string, object> items = new Dictionary<string, object>
			{
				["hello"] = new { prop = 12345, prop2 = "world" },
				["world"] = new { prop = 123 }
			};
			await UploadTestDataAsync(step.StepId, items);

			if (step.Name == "Stage FortniteClient Win64")
			{
				outcome = JobStepOutcome.Failure;
			}

			foreach (KeyValuePair<string, string> credential in step.Credentials)
			{
				logger.LogInformation("Credential: {CredentialName}={CredentialValue}", credential.Key, credential.Value);
			}

			PerforceLogger perforceLogger = new PerforceLogger(logger);
			perforceLogger.AddClientView(new DirectoryReference("D:\\Test"), "//UE4/Main/...", 12345);

			using (LogParser filter = new LogParser(perforceLogger, new List<string>()))
			{
				if(outcome == JobStepOutcome.Warnings)
				{
					filter.WriteLine("D:\\Test\\Path\\To\\Source\\File.cpp(234): warning: This is a compilation warning");
					logger.LogWarning("This is a warning!");
					filter.WriteLine("warning: this is a test");
				}
				if(outcome == JobStepOutcome.Failure)
				{
					filter.WriteLine("D:\\Test\\Path\\To\\Source\\File.cpp(234): error: This is a compilation error");
					logger.LogError("This is an error!");
					filter.WriteLine("error: this is a test");
				}
			}

			FileReference currentFile = new FileReference(Assembly.GetExecutingAssembly().Location);
			await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, step.StepId, currentFile.GetFileName(), currentFile, logger, cancellationToken);

			logger.LogInformation("**** FINISH NODE {StepName} ****", step.Name);

			return true;
		}

		public override Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Finalizing");
			return Task.CompletedTask;
		}
	}

	class TestExecutorFactory : JobExecutorFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly ILogger<TestExecutor> _logger;

		public override string Name => TestExecutor.Name;

		public TestExecutorFactory(IHttpClientFactory httpClientFactory, ILogger<TestExecutor> logger)
		{
			_httpClientFactory = httpClientFactory;
			_logger = logger;
		}

		public override JobExecutor CreateExecutor(ISession session, ExecuteJobTask executeJobTask, BeginBatchResponse beginBatchResponse)
		{
			return new TestExecutor(session, executeJobTask.JobId, executeJobTask.BatchId, beginBatchResponse.AgentType, _httpClientFactory, _logger);
		}
	}
}
