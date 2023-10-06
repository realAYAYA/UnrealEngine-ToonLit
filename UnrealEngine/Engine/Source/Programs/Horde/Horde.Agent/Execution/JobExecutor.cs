// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Grpc.Core;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Horde.Common;
using Horde.Common.Rpc;
using Horde.Storage.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution
{
	interface IJobExecutor
	{
		Task InitializeAsync(ILogger logger, CancellationToken cancellationToken);
		Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken);
		Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken);
	}

	class JobExecutorOptions
	{
		public ISession Session { get; }
		public IServerStorageFactory StorageFactory { get; }
		public string JobId { get; }
		public string BatchId { get; }
		public BeginBatchResponse Batch { get; }
		public NamespaceId NamespaceId { get; }
		public string StoragePrefix { get; }
		public string Token { get; }
		public JobOptions JobOptions { get; }

		public JobExecutorOptions(ISession session, IServerStorageFactory storageFactory, string jobId, string batchId, BeginBatchResponse batch, NamespaceId namespaceId, string storagePrefix, string token, JobOptions jobOptions)
		{
			Session = session;
			StorageFactory = storageFactory;
			JobId = jobId;
			BatchId = batchId;
			Batch = batch;
			NamespaceId = namespaceId;
			StoragePrefix = storagePrefix;
			Token = token;
			JobOptions = jobOptions;
		}
	}

	abstract class JobExecutor : IJobExecutor
	{
		protected class ExportedNode
		{
			public string Name { get; set; } = String.Empty;
			public bool RunEarly { get; set; }
			public bool? Warnings { get; set; }
			public List<string> Inputs { get; set; } = new List<string>();
			public List<string> Outputs { get; set; } = new List<string>();
			public List<string> InputDependencies { get; set; } = new List<string>();
			public List<string> OrderDependencies { get; set; } = new List<string>();
			public Dictionary<string, string> Annotations { get; set; } = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		}

		protected class ExportedGroup
		{
			public List<string> Types { get; set; } = new List<string>();
			public List<ExportedNode> Nodes { get; set; } = new List<ExportedNode>();
		}

		protected class ExportedAggregate
		{
			public string Name { get; set; } = String.Empty;
			public List<string> Nodes { get; set; } = new List<string>();
		}

		protected class ExportedLabel
		{
			public string? Name { get; set; }
			public string? Category { get; set; }
			public string? UgsBadge { get; set; }
			public string? UgsProject { get; set; }
			public LabelChange Change { get; set; } = LabelChange.Current;
			public List<string> RequiredNodes { get; set; } = new List<string>();
			public List<string> IncludedNodes { get; set; } = new List<string>();
		}

		protected class ExportedBadge
		{
			public string Name { get; set; } = String.Empty;
			public string? Project { get; set; }
			public int Change { get; set; }
			public string? Dependencies { get; set; }
		}

		protected class ExportedGraph
		{
			public List<ExportedGroup> Groups { get; set; } = new List<ExportedGroup>();
			public List<ExportedAggregate> Aggregates { get; set; } = new List<ExportedAggregate>();
			public List<ExportedLabel> Labels { get; set; } = new List<ExportedLabel>();
			public List<ExportedBadge> Badges { get; set; } = new List<ExportedBadge>();
		}

		class TraceEvent
		{
			public string Name { get; set; } = "Unknown";
			public string? SpanId { get; set; }
			public string? ParentId { get; set; }
			public string? Service { get; set; }
			public string? Resource { get; set; }
			public DateTimeOffset StartTime { get; set; }
			public DateTimeOffset FinishTime { get; set; }
			public Dictionary<string, string>? Metadata { get; set; }

			[JsonIgnore]
			public int Index { get; set; }
		}

		class TraceEventList
		{
			public List<TraceEvent> Spans { get; set; } = new List<TraceEvent>();
		}

		class TraceSpan
		{
			public string? Name { get; set; }
			public string? SpanId { get; set; }
			public string? ParentId { get; set; }
			public string? Service { get; set; }
			public string? Resource { get; set; }
			public long Start { get; set; }
			public long Finish { get; set; }
			public Dictionary<string, string>? Properties { get; set; }
			public List<TraceSpan>? Children { get; set; }

			public void AddChild(TraceSpan child)
			{
				Children ??= new List<TraceSpan>();
				Children.Add(child);
			}
		}

		class TestDataItem
		{
			public string Key { get; set; } = String.Empty;
			public Dictionary<string, object> Data { get; set; } = new Dictionary<string, object>();
		}

		class TestData
		{
			public List<TestDataItem> Items { get; set; } = new List<TestDataItem>();
		}

		class ReportData
		{
			public ReportScope Scope { get; set; }
			public ReportPlacement Placement { get; set; }
			public string Name { get; set; } = String.Empty;
			public string FileName { get; set; } = String.Empty;
		}

		const string ScriptArgumentPrefix = "-Script=";
		const string TargetArgumentPrefix = "-Target=";

		const string PreprocessedScript = "Engine/Saved/Horde/Preprocessed.xml";
		const string PreprocessedSchema = "Engine/Saved/Horde/Preprocessed.xsd";

		protected List<string> _targets = new List<string>();
		protected string? _scriptFileName;
		protected bool _preprocessScript;

		/// <summary>
		/// Logger for the local agent process (as opposed to job logger)
		/// </summary>
		protected readonly ILogger _logger;

		protected readonly string _jobId;
		protected readonly string _batchId;
		protected readonly BeginBatchResponse _batch;

		protected List<string> _additionalArguments = new List<string>();

		protected bool _compileAutomationTool = true;

		protected readonly ISession _session;
		protected readonly IServerStorageFactory _storageFactory;
		private readonly NamespaceId _namespaceId;
		private readonly string _storagePrefix;
		private readonly string _token;
		protected readonly JobOptions _jobOptions;

		protected IRpcConnection RpcConnection => _session.RpcConnection;
		protected Dictionary<string, string> _remapAgentTypes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		protected Dictionary<string, string> _envVars = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		private XgeMetadataExtractor? _xgeMetadataExtractor;

		public JobExecutor(JobExecutorOptions options, ILogger logger)
		{
			_session = options.Session;
			_storageFactory = options.StorageFactory;

			_jobId = options.JobId;
			_batchId = options.BatchId;
			_batch = options.Batch;

			_namespaceId = options.NamespaceId;
			_storagePrefix = options.StoragePrefix;
			_token = options.Token;
			_jobOptions = options.JobOptions;

			_envVars["UE_HORDE_URL"] = options.Session.ServerUrl.ToString();
			_envVars["UE_HORDE_TOKEN"] = options.Token;

			_logger = logger;
		}

		public virtual Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			// Setup the agent type
			foreach (KeyValuePair<string, string> envVar in _batch.Environment)
			{
				_envVars[envVar.Key] = envVar.Value;
			}

			// Figure out if we're running as an admin
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				if (IsUserAdministrator())
				{
					logger.LogInformation("Running as an elevated user.");
				}
				else
				{
					logger.LogInformation("Running as an restricted user.");
				}
			}

			// Get the BuildGraph arguments
			foreach (string argument in _batch.Arguments)
			{
				const string RemapAgentTypesPrefix = "-RemapAgentTypes=";
				if (argument.StartsWith(RemapAgentTypesPrefix, StringComparison.OrdinalIgnoreCase))
				{
					foreach (string map in argument.Substring(RemapAgentTypesPrefix.Length).Split(','))
					{
						int colonIdx = map.IndexOf(':', StringComparison.Ordinal);
						if (colonIdx != -1)
						{
							_remapAgentTypes[map.Substring(0, colonIdx)] = map.Substring(colonIdx + 1);
						}
					}
				}
				else if (argument.StartsWith(ScriptArgumentPrefix, StringComparison.OrdinalIgnoreCase))
				{
					_scriptFileName = argument.Substring(ScriptArgumentPrefix.Length);
				}
				else if (argument.Equals("-Preprocess", StringComparison.OrdinalIgnoreCase))
				{
					_preprocessScript = true;
				}
				else if (argument.StartsWith(TargetArgumentPrefix, StringComparison.OrdinalIgnoreCase))
				{
					_targets.Add(argument.Substring(TargetArgumentPrefix.Length));
				}
				else
				{
					_additionalArguments.Add(argument);
				}
			}
			if (_batch.PreflightChange != 0)
			{
				_additionalArguments.Add($"-set:PreflightChange={_batch.PreflightChange}");
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? xgeDir = XgeMetadataExtractor.FindXgeDir();
				if (xgeDir != null)
				{
					_xgeMetadataExtractor = new XgeMetadataExtractor(xgeDir);
				}
				else
				{
					_logger.LogInformation("Unable to locate XGE directory. Not installed?");
				}
			}

			return Task.CompletedTask;
		}

		public static bool IsUserAdministrator()
		{
			if (!OperatingSystem.IsWindows())
			{
				return false;
			}

			try
			{
				using(WindowsIdentity identity = WindowsIdentity.GetCurrent())
				{
					WindowsPrincipal principal = new WindowsPrincipal(identity);
					return principal.IsInRole(WindowsBuiltInRole.Administrator);
				}
			}
			catch
			{
				return false;
			}
		}

		public virtual async Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			if (step.Name == "Setup Build")
			{
				if (await SetupAsync(step, logger, cancellationToken))
				{
					return JobStepOutcome.Success;
				}
				else
				{
					return JobStepOutcome.Failure;
				}
			}
			else
			{
				if (await ExecuteAsync(step, logger, cancellationToken))
				{
					return JobStepOutcome.Success;
				}
				else
				{
					return JobStepOutcome.Failure;
				}
			}
		}

		public virtual Task FinalizeAsync(ILogger jobLogger, CancellationToken cancellationToken)
		{
			return Task.CompletedTask;
		}

		static DirectoryReference GetAutomationToolDir(DirectoryReference sharedStorageDir)
		{
			return DirectoryReference.Combine(sharedStorageDir, "UAT");
		}

		protected void CopyAutomationTool(DirectoryReference sharedStorageDir, DirectoryReference workspaceDir, ILogger logger)
		{
			DirectoryReference buildDir = GetAutomationToolDir(sharedStorageDir);

			FileReference[] automationToolPaths = new FileReference[]
			{
				FileReference.Combine(buildDir, "Engine", "Binaries", "DotNET", "AutomationTool.exe"),
				FileReference.Combine(buildDir, "Engine", "Binaries", "DotNET", "AutomationTool", "AutomationTool.exe")
			};

			if (automationToolPaths.Any(automationTool => FileReference.Exists(automationTool)))
			{
				logger.LogInformation("Copying AutomationTool binaries from '{BuildDir}' to '{WorkspaceDir}", buildDir, workspaceDir);
				foreach (FileReference sourceFile in DirectoryReference.EnumerateFiles(buildDir, "*", SearchOption.AllDirectories))
				{
					FileReference targetFile = FileReference.Combine(workspaceDir, sourceFile.MakeRelativeTo(buildDir));
					if (FileReference.Exists(targetFile))
					{
						FileUtils.ForceDeleteFile(targetFile);
					}
					DirectoryReference.CreateDirectory(targetFile.Directory);
					FileReference.Copy(sourceFile, targetFile);
				}
				_compileAutomationTool = false;
			}
		}

		protected static void DeleteCachedBuildGraphManifests(DirectoryReference workspaceDir, ILogger logger)
		{
			DirectoryReference manifestDir = DirectoryReference.Combine(workspaceDir, "Engine", "Saved", "BuildGraph");
			if (DirectoryReference.Exists(manifestDir))
			{
				try
				{
					FileUtils.ForceDeleteDirectoryContents(manifestDir);
				}
				catch (Exception ex)
				{
					logger.LogWarning(ex, "Unable to delete contents of {ManifestDir}", manifestDir.ToString());
				}
			}
		}

		private async Task StorePreprocessedFile(FileReference? localFile, string stepId, DirectoryReference? sharedStorageDir, ILogger logger, CancellationToken cancellationToken)
		{
			if (localFile != null)
			{
				string fileName = localFile.GetFileName();
				await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, stepId, fileName, localFile, logger, cancellationToken);

				if (sharedStorageDir != null)
				{
					FileReference remoteFile = FileReference.Combine(sharedStorageDir, fileName);
					DirectoryReference.CreateDirectory(remoteFile.Directory);
					FileReference.Copy(localFile, remoteFile);
				}
			}
		}

		private static void FetchPreprocessedFile(FileReference localFile, DirectoryReference? sharedStorageDir, ILogger logger)
		{
			if (!FileReference.Exists(localFile))
			{
				if (sharedStorageDir == null)
				{
					throw new FileNotFoundException($"Missing preprocessed script {localFile}");
				}

				FileReference remoteFile = FileReference.Combine(sharedStorageDir, localFile.GetFileName());
				logger.LogInformation("Copying {RemoteFile} to {LocalFile}", remoteFile, localFile);
				DirectoryReference.CreateDirectory(localFile.Directory);
				FileReference.Copy(remoteFile, localFile, false);
			}
		}

		protected abstract Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken);

		protected abstract Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken);

		protected virtual async Task<bool> SetupAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, bool? useP4, ILogger logger, CancellationToken cancellationToken)
		{
			FileReference definitionFile = FileReference.Combine(workspaceDir, "Engine", "Saved", "Horde", "Exported.json");

			StringBuilder arguments = new StringBuilder($"BuildGraph");
			if (_scriptFileName != null)
			{
				arguments.AppendArgument(ScriptArgumentPrefix, _scriptFileName);
			}
			arguments.AppendArgument("-HordeExport=", definitionFile.FullName);
			arguments.AppendArgument("-ListOnly");
			//Arguments.AppendArgument("-TokenSignature=", JobId.ToString());
			foreach (string additionalArgument in _additionalArguments)
			{
				arguments.AppendArgument(additionalArgument);
			}

			FileReference? preprocessedScriptFile = null;
			FileReference? preprocessedSchemaFile = null;
			if (_preprocessScript)
			{
				preprocessedScriptFile = FileReference.Combine(workspaceDir, PreprocessedScript);
				arguments.AppendArgument("-Preprocess=", preprocessedScriptFile.FullName);

				preprocessedSchemaFile = FileReference.Combine(workspaceDir, PreprocessedSchema);
				arguments.AppendArgument("-Schema=", preprocessedSchemaFile.FullName);
			}
			if (sharedStorageDir != null && !_preprocessScript) // Do not precompile when preprocessing the script; other agents may have a different view of UAT
			{
				DirectoryReference buildDir = GetAutomationToolDir(sharedStorageDir);
				arguments.Append($" CopyUAT -WithLauncher -TargetDir=\"{buildDir}\"");
			}

			int result = await ExecuteAutomationToolAsync(step, workspaceDir, sharedStorageDir, arguments.ToString(), useP4, logger, cancellationToken);
			if (result != 0)
			{
				return false;
			}
			
			await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, step.StepId, definitionFile.GetFileName(), definitionFile, logger, cancellationToken);
			await StorePreprocessedFile(preprocessedScriptFile, step.StepId, sharedStorageDir, logger, cancellationToken);
			await StorePreprocessedFile(preprocessedSchemaFile, step.StepId, sharedStorageDir, logger, cancellationToken);

			JsonSerializerOptions options = new JsonSerializerOptions();
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());

			ExportedGraph graph = JsonSerializer.Deserialize<ExportedGraph>(await FileReference.ReadAllBytesAsync(definitionFile, cancellationToken), options)!;

			List<string> missingAgentTypes = new List<string>();

			UpdateGraphRequest updateGraph = new UpdateGraphRequest();
			updateGraph.JobId = _jobId;
			foreach (ExportedGroup exportedGroup in graph.Groups)
			{
				string? agentTypeName = null;
				foreach (string validAgentTypeName in exportedGroup.Types)
				{
					string? thisAgentTypeName;
					if (!_remapAgentTypes.TryGetValue(validAgentTypeName, out thisAgentTypeName))
					{
						thisAgentTypeName = validAgentTypeName;
					}

					if (_batch.ValidAgentTypes.Contains(thisAgentTypeName))
					{
						agentTypeName = thisAgentTypeName;
						break;
					}
				}

				if (agentTypeName == null)
				{
					agentTypeName = exportedGroup.Types.FirstOrDefault() ?? "Unspecified";
					foreach (ExportedNode node in exportedGroup.Nodes)
					{
						missingAgentTypes.Add($"  {node.Name} ({String.Join(", ", exportedGroup.Types)})");
					}
				}

				CreateGroupRequest createGroup = new CreateGroupRequest();
				createGroup.AgentType = agentTypeName;

				foreach (ExportedNode exportedNode in exportedGroup.Nodes)
				{
					CreateNodeRequest createNode = new CreateNodeRequest();
					createNode.Name = exportedNode.Name;
					if (exportedNode.Inputs != null)
					{
						createNode.Inputs.Add(exportedNode.Inputs);
					}
					if (exportedNode.Outputs != null)
					{
						createNode.Outputs.Add(exportedNode.Outputs);
					}
					if (exportedNode.InputDependencies != null)
					{
						createNode.InputDependencies.Add(exportedNode.InputDependencies);
					}
					if (exportedNode.OrderDependencies != null)
					{
						createNode.OrderDependencies.Add(exportedNode.OrderDependencies);
					}
					createNode.RunEarly = exportedNode.RunEarly;
					createNode.Warnings = exportedNode.Warnings;
					createNode.Priority = Priority.Normal;
					createNode.Annotations.Add(exportedNode.Annotations);
					createGroup.Nodes.Add(createNode);
				}
				updateGraph.Groups.Add(createGroup);
			}

			if (missingAgentTypes.Count > 0)
			{
				logger.LogInformation("The following nodes cannot be executed in this stream due to missing agent types:");
				foreach (string missingAgentType in missingAgentTypes)
				{
					logger.LogInformation("{Node}", missingAgentType);
				}
			}

			foreach (ExportedAggregate exportedAggregate in graph.Aggregates)
			{
				CreateAggregateRequest createAggregate = new CreateAggregateRequest();
				createAggregate.Name = exportedAggregate.Name;
				createAggregate.Nodes.AddRange(exportedAggregate.Nodes);
				updateGraph.Aggregates.Add(createAggregate);
			}

			foreach (ExportedLabel exportedLabel in graph.Labels)
			{
				CreateLabelRequest createLabel = new CreateLabelRequest();
				if (exportedLabel.Name != null)
				{
					createLabel.DashboardName = exportedLabel.Name;
				}
				if (exportedLabel.Category != null)
				{
					createLabel.DashboardCategory = exportedLabel.Category;
				}
				if (exportedLabel.UgsBadge != null)
				{
					createLabel.UgsName = exportedLabel.UgsBadge;
				}
				if (exportedLabel.UgsProject != null)
				{
					createLabel.UgsProject = exportedLabel.UgsProject;
				}
				createLabel.Change = exportedLabel.Change;
				createLabel.RequiredNodes.AddRange(exportedLabel.RequiredNodes);
				createLabel.IncludedNodes.AddRange(exportedLabel.IncludedNodes);
				updateGraph.Labels.Add(createLabel);
			}

			Dictionary<string, ExportedNode> nameToNode = graph.Groups.SelectMany(x => x.Nodes).ToDictionary(x => x.Name, x => x);
			foreach (ExportedBadge exportedBadge in graph.Badges)
			{
				CreateLabelRequest createLabel = new CreateLabelRequest();
				createLabel.UgsName = exportedBadge.Name;

				string? project = exportedBadge.Project;
				if (project != null && project.StartsWith("//", StringComparison.Ordinal))
				{
					int nextIdx = project.IndexOf('/', 2);
					if (nextIdx != -1)
					{
						nextIdx = project.IndexOf('/', nextIdx + 1);
						if (nextIdx != -1 && !project.Substring(nextIdx).Equals("/...", StringComparison.Ordinal))
						{
							createLabel.UgsProject = project.Substring(nextIdx + 1);
						}
					}
				}

				if (exportedBadge.Change == _batch.Change || exportedBadge.Change == 0)
				{
					createLabel.Change = LabelChange.Current;
				}
				else if (exportedBadge.Change == _batch.CodeChange)
				{
					createLabel.Change = LabelChange.Code;
				}
				else
				{
					logger.LogWarning("Badge is set to display for changelist {Change}. This is neither the current changelist ({CurrentChange}) or the current code changelist ({CurrentCodeChange}).", exportedBadge.Change, _batch.Change, _batch.CodeChange);
				}

				if (exportedBadge.Dependencies != null)
				{
					createLabel.RequiredNodes.AddRange(exportedBadge.Dependencies.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));

					HashSet<string> dependencies = new HashSet<string>();
					foreach(string requiredNode in createLabel.RequiredNodes)
					{
						GetRecursiveDependencies(requiredNode, nameToNode, dependencies);
					}
					createLabel.IncludedNodes.AddRange(dependencies);
				}
				updateGraph.Labels.Add(createLabel);
			}
			

			await RpcConnection.InvokeAsync((JobRpc.JobRpcClient x) => x.UpdateGraphAsync(updateGraph, null, null, cancellationToken), cancellationToken);

			HashSet<string> validTargets = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			validTargets.Add("Setup Build");
			validTargets.UnionWith(updateGraph.Groups.SelectMany(x => x.Nodes).Select(x => x.Name));
			validTargets.UnionWith(updateGraph.Aggregates.Select(x => x.Name));
			foreach (string target in _targets)
			{
				if (!validTargets.Contains(target))
				{
					logger.LogWarning("Target '{Target}' does not exist in the graph", target);
				}
			}

			return true;
		}
		
		private async Task UploadXgeMonitorFilesAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			if (_xgeMetadataExtractor != null && _jobOptions.CollectIbMonFilesAsArtifacts is true)
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("XgeMonitorFilesUpload").StartActive();
				List<FileReference> ibMonFiles = _xgeMetadataExtractor.GetLocalIbMonFilePaths();

				scope.Span.SetTag("NumIbMonFiles", ibMonFiles.Count);
				ParallelOptions options = new () { MaxDegreeOfParallelism = 5, CancellationToken = cancellationToken };
				await Parallel.ForEachAsync(ibMonFiles, options, async (file, innerCt) =>
				{
					string artifactName = "Xge/" + file.GetFileName()
						.Replace("{", "", StringComparison.Ordinal)
						.Replace("}", "", StringComparison.Ordinal);
					
					await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, step.StepId,
						artifactName, file, logger, innerCt);
				});
			}
		}

		private static void GetRecursiveDependencies(string name, Dictionary<string, ExportedNode> nameToNode, HashSet<string> dependencies)
		{
			ExportedNode? node;
			if (nameToNode.TryGetValue(name, out node) && dependencies.Add(node.Name))
			{
				foreach (string inputDependency in node.InputDependencies)
				{
					GetRecursiveDependencies(inputDependency, nameToNode, dependencies);
				}
			}
		}

		protected async Task<bool> ExecuteAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, bool? useP4, ILogger logger, CancellationToken cancellationToken)
		{
			StringBuilder arguments = new StringBuilder("BuildGraph");
			if (_preprocessScript)
			{
				FileReference localPreprocessedScript = FileReference.Combine(workspaceDir, PreprocessedScript);
				FetchPreprocessedFile(localPreprocessedScript, sharedStorageDir, logger);
				arguments.AppendArgument(ScriptArgumentPrefix, localPreprocessedScript.FullName);

				FileReference localPreprocessedSchema = FileReference.Combine(workspaceDir, PreprocessedSchema);
				FetchPreprocessedFile(localPreprocessedSchema, sharedStorageDir, logger);
				arguments.AppendArgument("-ImportSchema=", localPreprocessedSchema.FullName);
			}
			else if (_scriptFileName != null)
			{
				arguments.AppendArgument(ScriptArgumentPrefix, _scriptFileName);
			}
			arguments.AppendArgument("-SingleNode=", step.Name);
//			Arguments.AppendArgument("-TokenSignature=", JobId.ToString());

			foreach (string additionalArgument in _additionalArguments)
			{
				if (!_preprocessScript || !additionalArgument.StartsWith("-set:", StringComparison.OrdinalIgnoreCase))
				{
					arguments.AppendArgument(additionalArgument);
				}
			}

			if (_jobOptions.CollectIbMonFilesAsArtifacts is true)
			{
				_xgeMetadataExtractor?.ClearLocalIbMonFiles();	
			}
			
			if (_jobOptions.UseNewTempStorage ?? false)
			{
				bool result = await ExecuteWithTempStorageAsync(step, workspaceDir, arguments.ToString(), useP4, logger, cancellationToken);
				await UploadXgeMonitorFilesAsync(step, logger, cancellationToken);
				return result;
			}
			else
			{
				if (sharedStorageDir != null)
				{
					arguments.AppendArgument("-SharedStorageDir=", sharedStorageDir.FullName);
				}
				
				bool result = await ExecuteAutomationToolAsync(step, workspaceDir, sharedStorageDir, arguments.ToString(), useP4, logger, cancellationToken) == 0;
				await UploadXgeMonitorFilesAsync(step, logger, cancellationToken);
				return result;
			}
		}

		protected async Task CreateArtifactsAsync(string stepId, JobArtifactType type, DirectoryReference baseDir, IEnumerable<(string, FileReference)> files, ILogger logger, CancellationToken cancellationToken)
		{
			if (_jobOptions.UseNewTempStorage ?? false)
			{
				await CreateArtifactAsync(stepId, type, baseDir, files.Select(x => x.Item2), logger, cancellationToken);
			}
			else
			{
				await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, stepId, files, logger, CancellationToken.None);
			}
		}

		protected async Task CreateArtifactAsync(string stepId, JobArtifactType type, DirectoryReference baseDir, IEnumerable<FileReference> files, ILogger logger, CancellationToken cancellationToken)
		{
			try
			{
				using IRpcClientRef<JobRpc.JobRpcClient> jobRpc = await RpcConnection.GetClientRefAsync<JobRpc.JobRpcClient>(cancellationToken);

				CreateJobArtifactResponse artifact = await jobRpc.Client.CreateArtifactAsync(new CreateJobArtifactRequest { JobId = _jobId, StepId = stepId, Type = type }, cancellationToken: cancellationToken);
				_logger.LogInformation("Created artifact {ArtifactId} with ref {RefName} in ns {Namespace}", artifact.Id, artifact.RefName, artifact.NamespaceId);

				IStorageClient storage = _storageFactory.CreateStorageClient(_session, new NamespaceId(artifact.NamespaceId), artifact.Token);
				await using IStorageWriter writer = storage.CreateWriter(new RefName(artifact.RefName));

				DirectoryNode dir = new DirectoryNode();
				await dir.AddFilesAsync(baseDir, files, new ChunkingOptions(), writer, new CopyStatsLogger(logger), cancellationToken);

				await storage.WriteNodeAsync(new RefName(artifact.RefName), dir, cancellationToken: cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Error creating artifact '{Type}'", type);
			}
		}

		private async Task<bool> ExecuteWithTempStorageAsync(BeginStepResponse step, DirectoryReference workspaceDir, string arguments, bool? useP4, ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference manifestDir = DirectoryReference.Combine(workspaceDir, "Engine", "Saved", "BuildGraph");

			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions { });
			IStorageClient storage = _storageFactory.CreateStorageClient(_session, _namespaceId, _token);
			BundleReader reader = new BundleReader(storage, cache, _logger);

			// Create the mapping of tag names to file sets
			Dictionary<string, HashSet<FileReference>> tagNameToFileSet = new Dictionary<string, HashSet<FileReference>>();

			// Read all the input tags for this node, and build a list of referenced input storage blocks
			HashSet<TempStorageBlockRef> inputStorageBlocks = new HashSet<TempStorageBlockRef>();
			foreach (string input in step.Inputs)
			{
				int slashIdx = input.IndexOf('/', StringComparison.Ordinal);
				if (slashIdx == -1)
				{
					logger.LogError("Missing slash from node input: {Input}", input);
					return false;
				}

				string nodeName = input.Substring(0, slashIdx);
				string tagName = input.Substring(slashIdx + 1);

				TempStorageTagManifest fileList = await TempStorage.RetrieveTagAsync(storage, _storagePrefix, nodeName, tagName, manifestDir, logger, cancellationToken);
				tagNameToFileSet[tagName] = fileList.ToFileSet(workspaceDir);
				inputStorageBlocks.UnionWith(fileList.Blocks);
			}

			// Read the manifests for all the input storage blocks
			Dictionary<TempStorageBlockRef, TempStorageBlockManifest> inputManifests = new Dictionary<TempStorageBlockRef, TempStorageBlockManifest>();
			using (IScope scope = GlobalTracer.Instance.BuildSpan("TempStorage").WithTag("resource", "read").StartActive())
			{
				Stopwatch timer = Stopwatch.StartNew();
				scope.Span.SetTag("blocks", inputStorageBlocks.Count);
				foreach (TempStorageBlockRef inputStorageBlock in inputStorageBlocks)
				{
					TempStorageBlockManifest manifest = await TempStorage.RetrieveBlockAsync(storage, _storagePrefix, inputStorageBlock.NodeName, inputStorageBlock.OutputName, workspaceDir, manifestDir, logger, cancellationToken);
					inputManifests[inputStorageBlock] = manifest;
				}
				scope.Span.SetTag("size", inputManifests.Sum(x => x.Value.GetTotalSize()));
				logger.LogInformation("Download took {Time:n1}s", timer.Elapsed.TotalSeconds);
			}

			// Read all the input storage blocks, keeping track of which block each file came from
			Dictionary<FileReference, TempStorageBlockRef> fileToStorageBlock = new Dictionary<FileReference, TempStorageBlockRef>();
			foreach (KeyValuePair<TempStorageBlockRef, TempStorageBlockManifest> pair in inputManifests)
			{
				TempStorageBlockRef inputStorageBlock = pair.Key;
				foreach (FileReference file in pair.Value.Files.Select(x => x.ToFileReference(workspaceDir)))
				{
					TempStorageBlockRef? currentStorageBlock;
					if (fileToStorageBlock.TryGetValue(file, out currentStorageBlock) && !TempStorage.IsDuplicateBuildProduct(file))
					{
						logger.LogError("File '{File}' was produced by {InputBlock} and {CurrentBlock}", file, inputStorageBlock.ToString(), currentStorageBlock.ToString());
					}
					fileToStorageBlock[file] = inputStorageBlock;
				}
			}

			// Run UAT
			if (await ExecuteAutomationToolAsync(step, workspaceDir, null, arguments, useP4, logger, cancellationToken) != 0)
			{
				return false;
			}

			// Read all the output manifests
			foreach (string tagName in step.OutputNames)
			{
				FileReference tagFileListLocation = TempStorage.GetTagManifestLocation(manifestDir, step.Name, tagName);
				logger.LogInformation("Reading local file list from {File}", tagFileListLocation.FullName);

				TempStorageTagManifest fileList = TempStorageTagManifest.Load(tagFileListLocation);
				tagNameToFileSet[tagName] = fileList.ToFileSet(workspaceDir);
			}

			// Check that none of the inputs have been clobbered
			Dictionary<string, string> modifiedFiles = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
			foreach (TempStorageFile file in inputManifests.Values.SelectMany(x => x.Files))
			{
				string? message;
				if (!modifiedFiles.ContainsKey(file.RelativePath) && !file.Compare(workspaceDir, out message))
				{
					modifiedFiles.Add(file.RelativePath, message);
				}
			}
			if (modifiedFiles.Count > 0)
			{
				string modifiedFileList = "";
				if (modifiedFiles.Count < 100)
				{
					modifiedFileList = String.Join("\n", modifiedFiles.Select(x => x.Value));
				}
				else
				{
					modifiedFileList = String.Join("\n", modifiedFiles.Take(100).Select(x => x.Value));
					modifiedFileList += $"{Environment.NewLine}And {modifiedFiles.Count - 100} more.";
				}

				logger.LogError("Build product(s) from a previous step have been modified:\n{FileList}", modifiedFileList);
				return false;
			}

			// Determine all the output files which are required to be copied to temp storage (because they're referenced by nodes in another agent)
			HashSet<FileReference> referencedOutputFiles = new HashSet<FileReference>();
			foreach (int publishOutput in step.PublishOutputs)
			{
				string tagName = step.OutputNames[publishOutput];
				referencedOutputFiles.UnionWith(tagNameToFileSet[tagName]);
			}

			// Find a block name for all new outputs
			Dictionary<FileReference, string> fileToBlockName = new Dictionary<FileReference, string>();
			for (int idx = 0; idx < step.OutputNames.Count; idx++)
			{
				string tagName = step.OutputNames[idx];

				string outputNameWithoutHash = tagName.TrimStart('#');
				bool isDefaultOutput = outputNameWithoutHash.Equals(step.Name, StringComparison.OrdinalIgnoreCase);

				HashSet<FileReference> files = tagNameToFileSet[tagName];
				foreach (FileReference file in files)
				{
					if (!fileToStorageBlock.ContainsKey(file) && file.IsUnderDirectory(workspaceDir))
					{
						if (isDefaultOutput)
						{
							if (!fileToBlockName.ContainsKey(file))
							{
								fileToBlockName[file] = "";
							}
						}
						else
						{
							string? blockName;
							if (fileToBlockName.TryGetValue(file, out blockName) && blockName.Length > 0)
							{
								fileToBlockName[file] = $"{blockName}+{outputNameWithoutHash}";
							}
							else
							{
								fileToBlockName[file] = outputNameWithoutHash;
							}
						}
					}
				}
			}

			// Invert the dictionary to make a mapping of storage block to the files each contains
			Dictionary<string, HashSet<FileReference>> outputStorageBlockToFiles = new Dictionary<string, HashSet<FileReference>>();
			foreach (KeyValuePair<FileReference, string> pair in fileToBlockName)
			{
				HashSet<FileReference>? files;
				if (!outputStorageBlockToFiles.TryGetValue(pair.Value, out files))
				{
					files = new HashSet<FileReference>();
					outputStorageBlockToFiles.Add(pair.Value, files);
				}
				files.Add(pair.Key);
			}

			// Write all the storage blocks, and update the mapping from file to storage block
			using (GlobalTracer.Instance.BuildSpan("TempStorage").WithTag("resource", "Write").StartActive())
			{
				Stopwatch timer = Stopwatch.StartNew();
				RefName refName = TempStorage.GetRefNameForNode(_storagePrefix, step.Name);

				await using IStorageWriter treeWriter = storage.CreateWriter(refName);

				DirectoryNode outputNode = new DirectoryNode();

				// Create all the output blocks
				foreach (KeyValuePair<string, HashSet<FileReference>> pair in outputStorageBlockToFiles)
				{
					TempStorageBlockRef outputBlock = new TempStorageBlockRef(step.Name, pair.Key);
					foreach (FileReference file in pair.Value)
					{
						fileToStorageBlock.Add(file, outputBlock);
					}
					if (pair.Value.Any(x => referencedOutputFiles.Contains(x)))
					{
						outputNode.AddDirectory(await TempStorage.ArchiveBlockAsync(manifestDir, step.Name, pair.Key, workspaceDir, pair.Value.ToArray(), treeWriter, logger, cancellationToken));
					}
				}

				// Create all the output tags
				foreach (string outputName in step.OutputNames)
				{
					HashSet<FileReference> files = tagNameToFileSet[outputName];

					HashSet<TempStorageBlockRef> storageBlocks = new HashSet<TempStorageBlockRef>();
					foreach (FileReference file in files)
					{
						TempStorageBlockRef? storageBlock;
						if (fileToStorageBlock.TryGetValue(file, out storageBlock))
						{
							storageBlocks.Add(storageBlock);
						}
					}

					outputNode.AddFile(await TempStorage.ArchiveTagAsync(manifestDir, step.Name, outputName, workspaceDir, files, storageBlocks.ToArray(), treeWriter, logger, cancellationToken));
				}

				// Write the final node
				await storage.WriteNodeAsync(refName, outputNode, refOptions: new RefOptions(), cancellationToken: cancellationToken);
				logger.LogInformation("Upload took {Time:n1}s", timer.Elapsed.TotalSeconds);

				// Create the artifact
				using IRpcClientRef<JobRpc.JobRpcClient> jobRpc = await RpcConnection.GetClientRefAsync<JobRpc.JobRpcClient>(cancellationToken);

				CreateJobArtifactRequest artifactRequest = new CreateJobArtifactRequest();
				artifactRequest.JobId = _jobId;
				artifactRequest.StepId = step.StepId;
				artifactRequest.Type = JobArtifactType.Output;
				artifactRequest.NamespaceId = _namespaceId.ToString();
				artifactRequest.RefName = refName.ToString();

				CreateJobArtifactResponse artifact = await jobRpc.Client.CreateArtifactAsync(artifactRequest, cancellationToken: cancellationToken);
				logger.LogInformation("Created output artifact {ArtifactId} with ref {RefName} in namespace {Namespace}", artifact.Id, artifact.RefName, artifact.NamespaceId);
			}

			return true;
		}

		protected async Task<int> ExecuteAutomationToolAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, string? arguments, bool? useP4, ILogger logger, CancellationToken cancellationToken)
		{
			int result;
			using IScope scope = GlobalTracer.Instance.BuildSpan("BuildGraph").StartActive();
			
			if (!_compileAutomationTool)
			{
				arguments += " -NoCompile";
			}
			
			if (useP4 is false)
			{
				arguments += " -NoP4";
			}

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				result = await ExecuteCommandAsync(step, workspaceDir, sharedStorageDir, Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe", $"/C \"\"{workspaceDir}\\Engine\\Build\\BatchFiles\\RunUAT.bat\" {arguments}\"", logger, cancellationToken);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				string args = $"\"{workspaceDir}/Engine/Build/BatchFiles/RunUAT.sh\" {arguments}";
				
				if (_jobOptions.UseWine is true)
				{
					args = $"\"{workspaceDir}/Engine/Build/BatchFiles/RunWineUAT.sh\" {arguments}";
				}
				
				result = await ExecuteCommandAsync(step, workspaceDir, sharedStorageDir, "/bin/bash", args, logger, cancellationToken);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				result = await ExecuteCommandAsync(step, workspaceDir, sharedStorageDir, "/bin/sh", $"\"{workspaceDir}/Engine/Build/BatchFiles/RunUAT.sh\" {arguments}", logger, cancellationToken);
			}
			else
			{
				throw new Exception("Unsupported platform");
			}

			_compileAutomationTool = false;
			return result;
		}

		static void AddRestrictedDirs(List<DirectoryReference> directories, string subFolder)
		{
			int numDirs = directories.Count;
			for (int idx = 0; idx < numDirs; idx++)
			{
				DirectoryReference subDir = DirectoryReference.Combine(directories[idx], subFolder);
				if(DirectoryReference.Exists(subDir))
				{
					directories.AddRange(DirectoryReference.EnumerateDirectories(subDir));
				}
			}
		}

		static async Task<List<string>> ReadIgnorePatternsAsync(DirectoryReference workspaceDir, ILogger logger)
		{
			List<DirectoryReference> baseDirs = new List<DirectoryReference>();
			baseDirs.Add(DirectoryReference.Combine(workspaceDir, "Engine"));
			AddRestrictedDirs(baseDirs, "Restricted");
			AddRestrictedDirs(baseDirs, "Platforms");

			List<string> ignorePatternLines = new List<string>(Properties.Resources.IgnorePatterns.Split('\n', StringSplitOptions.RemoveEmptyEntries));
			foreach (DirectoryReference baseDir in baseDirs)
			{
				FileReference ignorePatternFile = FileReference.Combine(baseDir, "Build", "Horde", "IgnorePatterns.txt");
				if (FileReference.Exists(ignorePatternFile))
				{
					logger.LogInformation("Reading ignore patterns from {File}...", ignorePatternFile);
					ignorePatternLines.AddRange(await FileReference.ReadAllLinesAsync(ignorePatternFile));
				}
			}

			HashSet<string> ignorePatterns = new HashSet<string>(StringComparer.Ordinal);
			foreach (string line in ignorePatternLines)
			{
				string trimLine = line.Trim();
				if (trimLine.Length > 0 && trimLine[0] != '#')
				{
					ignorePatterns.Add(trimLine);
				}
			}

			return ignorePatterns.ToList();
		}

		static FileReference GetCleanupScript(DirectoryReference workspaceDir)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return FileReference.Combine(workspaceDir, "Cleanup.bat");
			}
			else
			{
				return FileReference.Combine(workspaceDir, "Cleanup.sh");
			}
		}

		static FileReference GetLeaseCleanupScript(DirectoryReference workspaceDir)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return FileReference.Combine(workspaceDir, "CleanupLease.bat");
			}
			else
			{
				return FileReference.Combine(workspaceDir, "CleanupLease.sh");
			}
		}

		internal IReadOnlyDictionary<string, string> GetEnvVars()
		{
			return new Dictionary<string, string>(_envVars);
		}

		protected static async Task ExecuteLeaseCleanupScriptAsync(DirectoryReference workspaceDir, ILogger logger)
		{
			using (LogParser parser = new LogParser(logger, new List<string>()))
			{
				FileReference leaseCleanupScript = GetLeaseCleanupScript(workspaceDir);
				await ExecuteCleanupScriptAsync(leaseCleanupScript, parser, logger);
			}
		}

		protected async Task TerminateProcessesAsync(TerminateCondition condition, ILogger logger)
		{
			try
			{
				await _session.TerminateProcessesAsync(condition, logger, CancellationToken.None);
			}
			catch (Exception ex)
			{
				logger.LogWarning(ex, "Exception while terminating processes: {Message}", ex.Message);
			}
		}

		static async Task ExecuteCleanupScriptAsync(FileReference cleanupScript, LogParser filter, ILogger logger)
		{
			if (FileReference.Exists(cleanupScript))
			{
				filter.WriteLine($"Executing cleanup script: {cleanupScript}");

				string fileName;
				string arguments;
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					fileName = "C:\\Windows\\System32\\Cmd.exe";
					arguments = $"/C \"{cleanupScript}\"";
				}
				else
				{
					fileName = "/bin/sh";
					arguments = $"\"{cleanupScript}\"";
				}

				try
				{
					using (CancellationTokenSource cancellationSource = new CancellationTokenSource())
					{
						cancellationSource.CancelAfter(TimeSpan.FromSeconds(30.0));
						await ExecuteProcessAsync(fileName, arguments, null, filter, logger, cancellationSource.Token);
					}
					FileUtils.ForceDeleteFile(cleanupScript);
				}
				catch (OperationCanceledException)
				{
					filter.WriteLine("Cleanup script did not complete within allotted time. Aborting.");
				}
			}
		}

		static async Task<int> ExecuteProcessAsync(string fileName, string arguments, IReadOnlyDictionary<string, string>? newEnvironment, LogParser filter, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Executing {File} {Arguments}", fileName.QuoteArgument(), arguments);

			using (CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
			{
				using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
				{
					using (ManagedProcess process = new ManagedProcess(processGroup, fileName, arguments, null, newEnvironment, null, ProcessPriorityClass.Normal))
					{
						bool hasExited = false;
						try
						{
							async Task WaitForExitOrCancelAsync()
							{
								await process.WaitForExitAsync(cancellationToken);
								hasExited = true;
								cancellationSource.CancelAfter(TimeSpan.FromSeconds(60.0));
							}

							Task cancelTask = WaitForExitOrCancelAsync();
							await process.CopyToAsync((buffer, offset, length) => filter.WriteData(buffer.AsMemory(offset, length)), 4096, cancellationSource.Token);
							await cancelTask;
						}
						catch (OperationCanceledException) when (hasExited)
						{
							logger.LogWarning("Process exited without closing output pipes; they may have been inherited by a child process that is still running.");
						}
						return process.ExitCode;
					}
				}
			}
		}
		
		/// <summary>
		/// Execute a process inside a Linux container
		/// </summary>
		/// <param name="arguments">Arguments</param>
		/// <param name="mountDirs">Directories to mount inside the container for read/write</param>
		/// <param name="newEnvironment">Environment variables</param>
		/// <param name="filter">Log parser</param>
		/// <param name="logger">Logger</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns></returns>
		/// <exception cref="Exception"></exception>
		async Task<int> ExecuteProcessInContainerAsync(string arguments, List<DirectoryReference> mountDirs, IReadOnlyDictionary<string, string>? newEnvironment, LogParser filter, ILogger logger, CancellationToken cancellationToken)
		{
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				throw new Exception("Only Linux is supported for executing a process inside a container");
			}

			if (String.IsNullOrEmpty(_jobOptions.Container.ImageUrl))
			{
				throw new Exception("Image URL is null or empty");
			}

			// Default to "docker" executable and assume it's available on the path
			string executable = _jobOptions.Container.ContainerEngineExecutable ?? "docker";

			uint linuxUid = LinuxInterop.getuid();
			uint linuxGid = LinuxInterop.getgid();
			
			List<string> containerArgs = new()
				{
					"run",
					"--tty", // Allocate a pseudo-TTY
					$"--name horde-job-{_jobId}-{_batchId}", // Better name for debugging purposes
					"--rm", // Ensure container is removed after run
					$"--user {linuxUid}:{linuxGid}" // Run container as current user (important for mounted dirs)
				};

			foreach (DirectoryReference mountDir in mountDirs)
			{
				containerArgs.Add($"--volume {mountDir.FullName}:{mountDir.FullName}:rw");
			}

			if (newEnvironment != null)
			{
				string envFilePath = Path.GetTempFileName();
				StringBuilder sb = new();
				foreach ((string key, string value) in newEnvironment)
				{
					sb.AppendLine($"{key}={value}");
				}
				await File.WriteAllTextAsync(envFilePath, sb.ToString(), cancellationToken);
				containerArgs.Add("--env-file=" + envFilePath);
			}

			if (!String.IsNullOrEmpty(_jobOptions.Container.ExtraArguments))
			{
				containerArgs.Add(_jobOptions.Container.ExtraArguments);
			}
			
			containerArgs.Add(_jobOptions.Container.ImageUrl);
			string containerArgStr = String.Join(' ', containerArgs);
			arguments = containerArgStr + " " + arguments;
			
			logger.LogInformation("Executing {File} {Arguments} in container", executable.QuoteArgument(), arguments);
			
			// Skip forwarding of env vars as they are explicitly set above as arguments to container run
			return await ExecuteProcessAsync(executable, arguments, new Dictionary<string, string>(), filter, logger, cancellationToken);
		}

		private static List<DirectoryReference> GetContainerMountDirs(DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, IReadOnlyDictionary<string, string> envVars)
		{
			List<DirectoryReference> dirs = new();
			dirs.Add(workspaceDir);
			if (sharedStorageDir != null)
			{
				dirs.Add(sharedStorageDir);
			}
			if (envVars.TryGetValue("UE_SDKS_ROOT", out string? autoSdkDirPath))
			{
				dirs.Add(new DirectoryReference(autoSdkDirPath));
			}
			return dirs;
		}

		async Task<int> ExecuteCommandAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, string fileName, string arguments, ILogger jobLogger, CancellationToken cancellationToken)
		{
			// Method for expanding environment variable properties related to this step
			Dictionary<string, string> properties = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
			properties.Add("RootDir", workspaceDir.FullName);

			// Combine all the supplied environment variables together
			Dictionary<string, string> newEnvVars = new Dictionary<string, string>(StringComparer.Ordinal);
			foreach (KeyValuePair<string, string> envVar in _envVars)
			{
				newEnvVars[envVar.Key] = StringUtils.ExpandProperties(envVar.Value, properties);
			}
			foreach (KeyValuePair<string, string> envVar in step.EnvVars)
			{
				newEnvVars[envVar.Key] = StringUtils.ExpandProperties(envVar.Value, properties);
			}
			foreach (KeyValuePair<string, string> envVar in step.Credentials)
			{
				newEnvVars[envVar.Key] = envVar.Value;
			}

			// Add all the other Horde-specific variables
			newEnvVars["IsBuildMachine"] = "1";

			DirectoryReference logDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "Logs");
			FileUtils.ForceDeleteDirectoryContents(logDir);
			newEnvVars["uebp_LogFolder"] = logDir.FullName;

			DirectoryReference telemetryDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "Telemetry");
			FileUtils.ForceDeleteDirectoryContents(telemetryDir);
			newEnvVars["UE_TELEMETRY_DIR"] = telemetryDir.FullName;

			DirectoryReference testDataDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "TestData");
			FileUtils.ForceDeleteDirectoryContents(testDataDir);
			newEnvVars["UE_TESTDATA_DIR"] = testDataDir.FullName;

			newEnvVars["UE_HORDE_JOBID"] = _jobId;
			newEnvVars["UE_HORDE_BATCHID"] = _batchId;
			newEnvVars["UE_HORDE_STEPID"] = step.StepId;

			// Enable structured logging output
			newEnvVars["UE_LOG_JSON_TO_STDOUT"] = "1";

			// Pass the location of the cleanup script to the job
			FileReference cleanupScript = GetCleanupScript(workspaceDir);
			newEnvVars["UE_HORDE_CLEANUP"] = cleanupScript.FullName;

			// Pass the location of the cleanup script to the job
			FileReference leaseCleanupScript = GetLeaseCleanupScript(workspaceDir);
			newEnvVars["UE_HORDE_LEASE_CLEANUP"] = leaseCleanupScript.FullName;

			// Set up the shared working dir
			newEnvVars["UE_HORDE_SHARED_DIR"] = DirectoryReference.Combine(_session.WorkingDir, "Saved").FullName;

			// Disable the S3DDC. This is technically a Fortnite-specific setting, but affects a large number of branches and is hard to retrofit. 
			// Setting here for now, since it's likely to be temporary.
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				newEnvVars["UE-S3DataCachePath"] = "None";
			}

			// Log all the environment variables to the log
			HashSet<string> credentialKeys = new HashSet<string>(step.Credentials.Keys, StringComparer.OrdinalIgnoreCase);
			foreach (KeyValuePair<string, string> envVar in newEnvVars.OrderBy(x => x.Key))
			{
				string value = "[redacted]";
				if (!credentialKeys.Contains(envVar.Key))
				{
					value = envVar.Value;
				}
				jobLogger.LogInformation("Setting env var: {Key}={Value}", envVar.Key, value);
			}

			// Add all the old environment variables into the list
			foreach (object? envVar in Environment.GetEnvironmentVariables())
			{
				System.Collections.DictionaryEntry entry = (System.Collections.DictionaryEntry)envVar!;
				string key = entry.Key.ToString()!;
				if (!newEnvVars.ContainsKey(key))
				{
					newEnvVars[key] = entry.Value!.ToString()!;
				}
			}

			// Clear out the telemetry directory
			if (DirectoryReference.Exists(telemetryDir))
			{
				FileUtils.ForceDeleteDirectoryContents(telemetryDir);
			}

			List<string> ignorePatterns = await ReadIgnorePatternsAsync(workspaceDir, jobLogger);

			int exitCode;
			using (LogParser filter = new LogParser(jobLogger, ignorePatterns))
			{
				await ExecuteCleanupScriptAsync(cleanupScript, filter, jobLogger);
				try
				{
					if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux) && _jobOptions?.Container?.Enabled is true)
					{
						List<DirectoryReference> mountDirs = GetContainerMountDirs(workspaceDir, sharedStorageDir, newEnvVars);
						exitCode = await ExecuteProcessInContainerAsync(arguments, mountDirs, newEnvVars, filter, jobLogger, cancellationToken);
					}
					else
					{
						exitCode = await ExecuteProcessAsync(fileName, arguments, newEnvVars, filter, jobLogger, cancellationToken);	
					}
				}
				finally
				{
					await ExecuteCleanupScriptAsync(cleanupScript, filter, jobLogger);
				}
				filter.Flush();
			}

			if (DirectoryReference.Exists(telemetryDir))
			{
				List<(string, FileReference)> telemetryFiles = new List<(string, FileReference)>();
				try
				{
					await CreateTelemetryFilesAsync(step.Name, telemetryDir, telemetryFiles, jobLogger, cancellationToken);
				}
				catch (Exception ex)
				{
					jobLogger.LogInformation(ex, "Unable to parse/upload telemetry files: {Message}", ex.Message);
				}

				await CreateArtifactsAsync(step.StepId, JobArtifactType.Trace, workspaceDir, telemetryFiles, jobLogger, cancellationToken);

				foreach ((_, FileReference telemetryFile) in telemetryFiles)
				{
					FileUtils.ForceDeleteFile(telemetryFile);
				}
			}

			if (DirectoryReference.Exists(testDataDir))
			{
				List<(string, FileReference)> testDataFiles = new List<(string, FileReference)>();

				Dictionary<string, object> combinedTestData = new Dictionary<string, object>();
				foreach (FileReference testDataFile in DirectoryReference.EnumerateFiles(testDataDir, "*.json", SearchOption.AllDirectories))
				{
					jobLogger.LogInformation("Reading test data {TestDataFile}", testDataFile);
					testDataFiles.Add(($"TestData/{testDataFile.MakeRelativeTo(testDataDir)}", testDataFile));

					TestData testData;
					using (FileStream stream = FileReference.Open(testDataFile, FileMode.Open))
					{
						JsonSerializerOptions options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
						testData = (await JsonSerializer.DeserializeAsync<TestData>(stream, options, cancellationToken))!;
					}

					foreach (TestDataItem item in testData.Items)
					{
						if (combinedTestData.ContainsKey(item.Key))
						{
							jobLogger.LogWarning("Key '{Key}' already exists - ignoring", item.Key);
						}
						else
						{
							jobLogger.LogDebug("Adding data with key '{Key}'", item.Key);
							combinedTestData.Add(item.Key, item.Data);
						}
					}
				}

				jobLogger.LogInformation("Found {NumResults} test results", combinedTestData.Count);
				await UploadTestDataAsync(step.StepId, combinedTestData);

				await CreateArtifactsAsync(step.StepId, JobArtifactType.TestData, workspaceDir, testDataFiles, jobLogger, cancellationToken);
			}

			if (DirectoryReference.Exists(logDir))
			{
				List<FileReference> artifactFiles = DirectoryReference.EnumerateFiles(logDir, "*", SearchOption.AllDirectories).ToList();
				if (_jobOptions.UseNewTempStorage ?? false)
				{
					await CreateArtifactAsync(step.StepId, JobArtifactType.Saved, workspaceDir, artifactFiles, jobLogger, cancellationToken);
				}
				else
				{
					Dictionary<FileReference, string> artifactFileToId = new Dictionary<FileReference, string>();
					foreach (FileReference artifactFile in artifactFiles)
					{
						string artifactName = artifactFile.MakeRelativeTo(logDir);

						string? artifactId = await ArtifactUploader.UploadAsync(RpcConnection, _jobId, _batchId, step.StepId, artifactName, artifactFile, jobLogger, cancellationToken);
						if (artifactId != null)
						{
							artifactFileToId[artifactFile] = artifactId;
						}
					}

					foreach (FileReference reportFile in artifactFileToId.Keys.Where(x => x.HasExtension(".report.json")))
					{
						try
						{
							await CreateReportAsync(step.StepId, reportFile, artifactFileToId, jobLogger);
						}
						catch (Exception ex)
						{
							jobLogger.LogWarning("Unable to upload report: {Message}", ex.Message);
						}
					}
				}
			}

			return exitCode;
		}

		async Task CreateTelemetryFilesAsync(string stepName, DirectoryReference telemetryDir, List<(string, FileReference)> telemetryFiles, ILogger jobLogger, CancellationToken cancellationToken)
		{
			List<TraceEventList> telemetryList = new List<TraceEventList>();
			foreach (FileReference telemetryFile in DirectoryReference.EnumerateFiles(telemetryDir, "*.json"))
			{
				jobLogger.LogInformation("Reading telemetry from {File}", telemetryFile);
				byte[] data = await FileReference.ReadAllBytesAsync(telemetryFile, cancellationToken);

				TraceEventList telemetry = JsonSerializer.Deserialize<TraceEventList>(data.AsSpan())!;
				if (telemetry.Spans.Count > 0)
				{
					string defaultServiceName = telemetryFile.GetFileNameWithoutAnyExtensions();
					foreach (TraceEvent span in telemetry.Spans)
					{
						span.Service ??= defaultServiceName;
					}
					telemetryList.Add(telemetry);
				}

				telemetryFiles.Add(($"Telemetry/{telemetryFile.GetFileName()}", telemetryFile));
			}

			List<TraceEvent> telemetrySpans = new List<TraceEvent>();
			foreach (TraceEventList telemetryEventList in telemetryList.OrderBy(x => x.Spans.First().StartTime).ThenBy(x => x.Spans.Last().FinishTime))
			{
				foreach (TraceEvent span in telemetryEventList.Spans)
				{
					if (span.FinishTime - span.StartTime > TimeSpan.FromMilliseconds(1.0))
					{
						span.Index = telemetrySpans.Count;
						telemetrySpans.Add(span);
					}
				}
			}

			FileReference? traceFile = null;
			if (telemetrySpans.Count > 0)
			{
				TraceSpan rootSpan = new TraceSpan();
				rootSpan.Name = stepName;

				Stack<TraceSpan> stack = new Stack<TraceSpan>();
				stack.Push(rootSpan);

				List<TraceSpan> spansWithExplicitParent = new List<TraceSpan>();
				Dictionary<string, TraceSpan> spans = new Dictionary<string, TraceSpan>(StringComparer.OrdinalIgnoreCase);

				foreach (TraceEvent traceEvent in telemetrySpans.OrderBy(x => x.StartTime).ThenByDescending(x => x.FinishTime).ThenBy(x => x.Index))
				{
					TraceSpan newSpan = new TraceSpan();
					newSpan.Name = traceEvent.Name;
					newSpan.SpanId = traceEvent.SpanId;
					newSpan.ParentId = traceEvent.ParentId;
					newSpan.Service = traceEvent.Service;
					newSpan.Resource = traceEvent.Resource;
					newSpan.Start = traceEvent.StartTime.UtcTicks;
					newSpan.Finish = traceEvent.FinishTime.UtcTicks;
					if (traceEvent.Metadata != null && traceEvent.Metadata.Count > 0)
					{
						newSpan.Properties = traceEvent.Metadata;
					}

					if (!String.IsNullOrEmpty(newSpan.SpanId))
					{
						spans.Add(newSpan.SpanId, newSpan);
					}

					TraceSpan stackTop = stack.Peek();
					while (stack.Count > 1 && newSpan.Start >= stackTop.Finish)
					{
						stack.Pop();
						stackTop = stack.Peek();
					}

					if (String.IsNullOrEmpty(newSpan.ParentId))
					{
						if (stack.Count > 1 && newSpan.Finish > stackTop.Finish)
						{
							jobLogger.LogInformation("Trace event name='{Name}', service'{Service}', resource='{Resource}' has invalid finish time ({SpanFinish} < {StackFinish})", newSpan.Name, newSpan.Service, newSpan.Resource, newSpan.Finish, stackTop.Finish);
							newSpan.Finish = stackTop.Finish;
						}

						stackTop.AddChild(newSpan);
					}
					else
					{
						spansWithExplicitParent.Add(newSpan);
					}

					stack.Push(newSpan);
				}

				foreach (TraceSpan span in spansWithExplicitParent)
				{
					if (span.ParentId != null && spans.TryGetValue(span.ParentId, out TraceSpan? parentSpan))
					{
						parentSpan.AddChild(span);
					}
					else
					{
						_logger.LogInformation("Parent {ParentId} of span {SpanId} was not found.", span.ParentId, span.SpanId);
					}
				}

				rootSpan.Start = rootSpan.Children!.First().Start;
				rootSpan.Finish = rootSpan.Children!.Last().Finish;

				traceFile = FileReference.Combine(telemetryDir, "Trace.json");
				using (FileStream stream = FileReference.Open(traceFile, FileMode.Create))
				{
					JsonSerializerOptions options = new JsonSerializerOptions { DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };
					await JsonSerializer.SerializeAsync(stream, rootSpan, options, cancellationToken);
				}
				telemetryFiles.Add(("Trace.json", traceFile));

				CreateTracingData(GlobalTracer.Instance.ActiveSpan, rootSpan);
			}
		}

		private async Task CreateReportAsync(string stepId, FileReference reportFile, Dictionary<FileReference, string> artifactFileToId, ILogger logger)
		{
			byte[] data = await FileReference.ReadAllBytesAsync(reportFile);

			JsonSerializerOptions options = new JsonSerializerOptions();
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			ReportData report = JsonSerializer.Deserialize<ReportData>(data, options)!;

			if (String.IsNullOrEmpty(report.Name))
			{
				logger.LogWarning("Missing 'Name' field in report data");
				return;
			}
			if (String.IsNullOrEmpty(report.FileName))
			{
				logger.LogWarning("Missing 'FileName' field in report data");
				return;
			}

			FileReference artifactFile = FileReference.Combine(reportFile.Directory, report.FileName);
			if (!artifactFileToId.TryGetValue(artifactFile, out string? artifactId))
			{
				logger.LogWarning("Unable to find artifact id for {File}", artifactFile);
				return;
			}

			logger.LogInformation("Creating report for {File} using artifact {ArtifactId}", reportFile, artifactId);

			CreateReportRequest request = new CreateReportRequest();
			request.JobId = _jobId;
			request.BatchId = _batchId;
			request.StepId = stepId;
			request.Scope = report.Scope;
			request.Placement = report.Placement;
			request.Name = report.Name;
			request.ArtifactId = artifactId;
			await RpcConnection.InvokeAsync((JobRpc.JobRpcClient x) => x.CreateReportAsync(request), CancellationToken.None);
		}

		private ISpan CreateTracingData(ISpan parent, TraceSpan span)
		{
			ISpan newSpan = GlobalTracer.Instance.BuildSpan(span.Name)
				.AsChildOf(parent)
				.WithServiceName(span.Service)
				.WithResourceName(span.Resource)
				.WithStartTimestamp(new DateTime(span.Start, DateTimeKind.Utc))
				.Start();

			if (span.Properties != null)
			{
				foreach (KeyValuePair<string, string> pair in span.Properties)
				{
					newSpan.SetTag(pair.Key, pair.Value);
				}
			}
			if (span.Children != null)
			{
				foreach (TraceSpan child in span.Children)
				{
					CreateTracingData(newSpan, child);
				}
			}

			newSpan.Finish(new DateTime(span.Finish, DateTimeKind.Utc));
			return newSpan;
		}

		protected async Task UploadTestDataAsync(string jobStepId, IEnumerable<KeyValuePair<string, object>> testData)
		{
			if (testData.Any())
			{
				await RpcConnection.InvokeAsync((JobRpc.JobRpcClient x) => UploadTestDataAsync(x, jobStepId, testData), CancellationToken.None);
			}
		}

		async Task<bool> UploadTestDataAsync(JobRpc.JobRpcClient rpcClient, string jobStepId, IEnumerable<KeyValuePair<string, object>> pairs)
		{
			using (AsyncClientStreamingCall<UploadTestDataRequest, UploadTestDataResponse> call = rpcClient.UploadTestData())
			{
				foreach (KeyValuePair<string, object> pair in pairs)
				{
					JsonSerializerOptions options = new JsonSerializerOptions();
					options.PropertyNameCaseInsensitive = true;
					options.Converters.Add(new JsonStringEnumConverter());
					byte[] data = JsonSerializer.SerializeToUtf8Bytes(pair.Value, options);

					UploadTestDataRequest request = new UploadTestDataRequest();
					request.JobId = _jobId;
					request.JobStepId = jobStepId;
					request.Key = pair.Key;
					request.Value = Google.Protobuf.ByteString.CopyFrom(data);
					await call.RequestStream.WriteAsync(request);
				}
				await call.RequestStream.CompleteAsync();
				await call.ResponseAsync;
			}
			return true;
		}
	}

	interface IJobExecutorFactory
	{
		string Name { get; }

		IJobExecutor CreateExecutor(AgentWorkspace workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options);
	}
}
