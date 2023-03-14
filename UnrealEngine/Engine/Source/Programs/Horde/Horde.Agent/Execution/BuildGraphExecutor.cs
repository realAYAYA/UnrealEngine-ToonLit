// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using Grpc.Core;
using Horde.Agent.Execution.Interfaces;
using Horde.Agent.Parser;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution
{
	abstract class BuildGraphExecutor : IExecutor
	{
		protected class ExportedNode
		{
			public string Name { get; set; } = String.Empty;
			public bool RunEarly { get; set; }
			public bool? Warnings { get; set; }
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
			public string? Service { get; set; }
			public string? Resource { get; set; }
			public long Start { get; set; }
			public long Finish { get; set; }
			public Dictionary<string, string>? Properties { get; set; }
			public List<TraceSpan>? Children { get; set; }
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

		protected string _jobId;
		protected string _batchId;
		protected string _agentTypeName;

		protected GetJobResponse _job;
		protected GetStreamResponse _stream;
		protected GetAgentTypeResponse _agentType;

		protected List<string> _additionalArguments = new List<string>();

		protected bool _compileAutomationTool = true;

		protected IRpcConnection _rpcConnection;
		protected Dictionary<string, string> _remapAgentTypes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		public BuildGraphExecutor(IRpcConnection rpcConnection, string jobId, string batchId, string agentTypeName)
		{
			_rpcConnection = rpcConnection;

			_jobId = jobId;
			_batchId = batchId;
			_agentTypeName = agentTypeName;

			_job = null!;
			_stream = null!;
			_agentType = null!;
		}

		public virtual async Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			// Get the job settings
			_job = await _rpcConnection.InvokeAsync(x => x.GetJobAsync(new GetJobRequest(_jobId), null, null, cancellationToken), new RpcContext(), cancellationToken);

			// Get the stream settings
			_stream = await _rpcConnection.InvokeAsync(x => x.GetStreamAsync(new GetStreamRequest(_job.StreamId), null, null, cancellationToken), new RpcContext(), cancellationToken);

			// Get the agent type to determine how to configure this machine
			_agentType = _stream.AgentTypes.FirstOrDefault(x => x.Key == _agentTypeName).Value;
			if (_agentType == null)
			{
				_agentType = new GetAgentTypeResponse();
			}

			logger.LogInformation("Configured as agent type {AgentType}", _agentTypeName);

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
			foreach (string argument in _job.Arguments)
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
			if (_job.PreflightChange != 0)
			{
				_additionalArguments.Add($"-set:PreflightChange={_job.PreflightChange}");
			}
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

		public async Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
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

		public virtual Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
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

		private async Task StorePreprocessedFile(FileReference? localFile, string stepId, DirectoryReference? sharedStorageDir, ILogger logger, CancellationToken cancellationToken)
		{
			if (localFile != null)
			{
				string fileName = localFile.GetFileName();
				await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, stepId, fileName, localFile, logger, cancellationToken);

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

		protected virtual async Task<bool> SetupAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, IReadOnlyDictionary<string, string> envVars, ILogger logger, CancellationToken cancellationToken)
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

			int result = await ExecuteAutomationToolAsync(step, workspaceDir, arguments.ToString(), envVars, step.Credentials, logger, cancellationToken);
			if (result != 0)
			{
				return false;
			}
			
			await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, step.StepId, definitionFile.GetFileName(), definitionFile, logger, cancellationToken);
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

					if (_stream!.AgentTypes.ContainsKey(thisAgentTypeName))
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

				if (exportedBadge.Change == _job.Change || exportedBadge.Change == 0)
				{
					createLabel.Change = LabelChange.Current;
				}
				else if (exportedBadge.Change == _job.CodeChange)
				{
					createLabel.Change = LabelChange.Code;
				}
				else
				{
					logger.LogWarning("Badge is set to display for changelist {Change}. This is neither the current changelist ({CurrentChange}) or the current code changelist ({CurrentCodeChange}).", exportedBadge.Change, _job.Change, _job.CodeChange);
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
			

			await _rpcConnection.InvokeAsync(x => x.UpdateGraphAsync(updateGraph, null, null, cancellationToken), new RpcContext(), cancellationToken);

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

		protected async Task<bool> ExecuteAsync(BeginStepResponse step, DirectoryReference workspaceDir, DirectoryReference? sharedStorageDir, IReadOnlyDictionary<string, string> envVars, ILogger logger, CancellationToken cancellationToken)
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
			else if(_scriptFileName != null)
			{
				arguments.AppendArgument(ScriptArgumentPrefix, _scriptFileName);
			}
			arguments.AppendArgument("-SingleNode=", step.Name);
			if (sharedStorageDir != null)
			{
				arguments.AppendArgument("-SharedStorageDir=", sharedStorageDir.FullName);
			}
//			Arguments.AppendArgument("-TokenSignature=", JobId.ToString());
			foreach (string additionalArgument in _additionalArguments)
			{
				if (!_preprocessScript || !additionalArgument.StartsWith("-set:", StringComparison.OrdinalIgnoreCase))
				{
					arguments.AppendArgument(additionalArgument);
				}
			}

			if (step.EnvVars != null && step.EnvVars.Count > 0)
			{
				Dictionary<string, string> newEnvVars = new Dictionary<string, string>(envVars);
				foreach (KeyValuePair<string, string> envVar in step.EnvVars)
				{
					newEnvVars[envVar.Key] = envVar.Value;
				}
				envVars = newEnvVars;
			}

			return await ExecuteAutomationToolAsync(step, workspaceDir, arguments.ToString(), envVars, step.Credentials, logger, cancellationToken) == 0;
		}

		protected async Task<int> ExecuteAutomationToolAsync(BeginStepResponse step, DirectoryReference workspaceDir, string arguments, IReadOnlyDictionary<string, string> envVars, IReadOnlyDictionary<string, string> credentials, ILogger logger, CancellationToken cancellationToken)
		{
			int result;
			using (IScope scope = GlobalTracer.Instance.BuildSpan("BuildGraph").StartActive())
			{
				if (!_compileAutomationTool)
				{
					arguments += " -NoCompile";
				}

				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					result = await ExecuteCommandAsync(step, workspaceDir, Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe", $"/C \"\"{workspaceDir}\\Engine\\Build\\BatchFiles\\RunUAT.bat\" {arguments}\"", envVars, credentials, logger, cancellationToken);
				}
				else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
				{
					result = await ExecuteCommandAsync(step, workspaceDir, "/bin/bash", $"\"{workspaceDir}/Engine/Build/BatchFiles/RunUAT.sh\" {arguments}", envVars, credentials, logger, cancellationToken);
				}
				else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
				{
					result = await ExecuteCommandAsync(step, workspaceDir, "/bin/sh", $"\"{workspaceDir}/Engine/Build/BatchFiles/RunUAT.sh\" {arguments}", envVars, credentials, logger, cancellationToken);
				}
				else
				{
					throw new Exception("Unsupported platform");
				}

				_compileAutomationTool = false;
			}
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
			using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
			{
				using (ManagedProcess process = new ManagedProcess(processGroup, fileName, arguments, null, newEnvironment, null, ProcessPriorityClass.Normal))
				{
					await process.CopyToAsync((buffer, offset, length) => filter.WriteData(buffer.AsMemory(offset, length)), 4096, cancellationToken);
					process.WaitForExit();
					return process.ExitCode;
				}
			}
		}

		async Task<int> ExecuteCommandAsync(BeginStepResponse step, DirectoryReference workspaceDir, string fileName, string arguments, IReadOnlyDictionary<string, string> envVars, IReadOnlyDictionary<string, string> credentials, ILogger logger, CancellationToken cancellationToken)
		{
			Dictionary<string, string> newEnvironment = new Dictionary<string, string>(envVars);
			foreach (object? envVar in Environment.GetEnvironmentVariables())
			{
				System.Collections.DictionaryEntry entry = (System.Collections.DictionaryEntry)envVar!;
				string key = entry.Key.ToString()!;
				if (!newEnvironment.ContainsKey(key))
				{
					newEnvironment[key] = entry.Value!.ToString()!;
				}
			}
			foreach (KeyValuePair<string, string> envVar in _agentType.Environment)
			{
				logger.LogInformation("Setting env var: {Key}={Value}", envVar.Key, envVar.Value);
				newEnvironment[envVar.Key] = envVar.Value;
			}
			foreach (KeyValuePair<string, string> envVar in credentials)
			{
				logger.LogInformation("Setting env var: {Key}=[redacted]", envVar.Key);
				newEnvironment[envVar.Key] = envVar.Value;
			}

			newEnvironment["IsBuildMachine"] = "1";

			DirectoryReference logDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "Logs");
			FileUtils.ForceDeleteDirectoryContents(logDir);
			newEnvironment["uebp_LogFolder"] = logDir.FullName;

			DirectoryReference telemetryDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "Telemetry");
			FileUtils.ForceDeleteDirectoryContents(telemetryDir);
			newEnvironment["UE_TELEMETRY_DIR"] = telemetryDir.FullName;

			DirectoryReference testDataDir = DirectoryReference.Combine(workspaceDir, "Engine", "Programs", "AutomationTool", "Saved", "TestData");
			FileUtils.ForceDeleteDirectoryContents(testDataDir);
			newEnvironment["UE_TESTDATA_DIR"] = testDataDir.FullName;

			newEnvironment["UE_HORDE_JOBID"] = _jobId;
			newEnvironment["UE_HORDE_BATCHID"] = _batchId;
			newEnvironment["UE_HORDE_STEPID"] = step.StepId;

			// Enable structured logging output
			newEnvironment["UE_LOG_JSON_TO_STDOUT"] = "1";

			// Pass the location of the cleanup script to the job
			FileReference cleanupScript = GetCleanupScript(workspaceDir);
			newEnvironment["UE_HORDE_CLEANUP"] = cleanupScript.FullName;

			// Disable the S3DDC. This is technically a Fortnite-specific setting, but affects a large number of branches and is hard to retrofit. 
			// Setting here for now, since it's likely to be temporary.
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				newEnvironment["UE-S3DataCachePath"] = "None";
			}

			// Clear out the telemetry directory
			if (DirectoryReference.Exists(telemetryDir))
			{
				FileUtils.ForceDeleteDirectoryContents(telemetryDir);
			}

			List<string> ignorePatterns = await ReadIgnorePatternsAsync(workspaceDir, logger);

			int exitCode;
			using (LogParser filter = new LogParser(logger, ignorePatterns))
			{
				await ExecuteCleanupScriptAsync(cleanupScript, filter, logger);
				try
				{
					exitCode = await ExecuteProcessAsync(fileName, arguments, newEnvironment, filter, logger, cancellationToken);
				}
				finally
				{
					await ExecuteCleanupScriptAsync(cleanupScript, filter, logger);
				}
				filter.Flush();
			}

			if (DirectoryReference.Exists(telemetryDir))
			{
				List<TraceEventList> telemetryList = new List<TraceEventList>();
				foreach (FileReference telemetryFile in DirectoryReference.EnumerateFiles(telemetryDir, "*.json"))
				{
					logger.LogInformation("Reading telemetry from {File}", telemetryFile);
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

					await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, step.StepId, $"Telemetry/{telemetryFile.GetFileName()}", telemetryFile, logger, CancellationToken.None);
					FileUtils.ForceDeleteFile(telemetryFile);
				}

				List<TraceEvent> telemetrySpans = new List<TraceEvent>();
				foreach (TraceEventList telemetry in telemetryList.OrderBy(x => x.Spans.First().StartTime).ThenBy(x => x.Spans.Last().FinishTime))
				{
					foreach (TraceEvent span in telemetry.Spans)
					{
						if (span.FinishTime - span.StartTime > TimeSpan.FromMilliseconds(1.0))
						{
							span.Index = telemetrySpans.Count;
							telemetrySpans.Add(span);
						}
					}
				}

				if (telemetrySpans.Count > 0)
				{
					TraceSpan rootSpan = new TraceSpan();
					rootSpan.Name = step.Name;

					Stack<TraceSpan> stack = new Stack<TraceSpan>();
					stack.Push(rootSpan);

					foreach (TraceEvent traceEvent in telemetrySpans.OrderBy(x => x.StartTime).ThenByDescending(x => x.FinishTime).ThenBy(x => x.Index))
					{
						TraceSpan newSpan = new TraceSpan();
						newSpan.Name = traceEvent.Name;
						newSpan.Service = traceEvent.Service;
						newSpan.Resource = traceEvent.Resource;
						newSpan.Start = traceEvent.StartTime.UtcTicks;
						newSpan.Finish = traceEvent.FinishTime.UtcTicks;
						if (traceEvent.Metadata != null && traceEvent.Metadata.Count > 0)
						{
							newSpan.Properties = traceEvent.Metadata;
						}

						TraceSpan stackTop = stack.Peek();
						while (stack.Count > 1 && newSpan.Start >= stackTop.Finish)
						{
							stack.Pop();
							stackTop = stack.Peek();
						}

						if (stack.Count > 1 && newSpan.Finish > stackTop.Finish)
						{
							logger.LogInformation("Trace event name='{Name}', service'{Service}', resource='{Resource}' has invalid finish time ({SpanFinish} < {StackFinish})", newSpan.Name, newSpan.Service, newSpan.Resource, newSpan.Finish, stackTop.Finish);
							newSpan.Finish = stackTop.Finish;
						}

						if (stackTop.Children == null)
						{
							stackTop.Children = new List<TraceSpan>();
						}

						stackTop.Children.Add(newSpan);
						stack.Push(newSpan);
					}

					rootSpan.Start = rootSpan.Children!.First().Start;
					rootSpan.Finish = rootSpan.Children!.Last().Finish;

					FileReference traceFile = FileReference.Combine(telemetryDir, "Trace.json");
					using (FileStream stream = FileReference.Open(traceFile, FileMode.Create))
					{
						JsonSerializerOptions options = new JsonSerializerOptions { DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };
						await JsonSerializer.SerializeAsync(stream, rootSpan, options, cancellationToken);
					}
					await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, step.StepId, "Trace.json", traceFile, logger, CancellationToken.None);

					CreateTracingData(GlobalTracer.Instance.ActiveSpan, rootSpan);
				}
			}
					
			if (DirectoryReference.Exists(testDataDir))
			{
				Dictionary<string, object> combinedTestData = new Dictionary<string, object>();
				foreach (FileReference testDataFile in DirectoryReference.EnumerateFiles(testDataDir, "*.json", SearchOption.AllDirectories))
				{
					logger.LogInformation("Reading test data {TestDataFile}", testDataFile);
					await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, step.StepId, $"TestData/{testDataFile.MakeRelativeTo(testDataDir)}", testDataFile, logger, CancellationToken.None);

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
							logger.LogWarning("Key '{Key}' already exists - ignoring", item.Key);
						}
						else
						{
							logger.LogDebug("Adding data with key '{Key}'", item.Key);
							combinedTestData.Add(item.Key, item.Data);
						}
					}
				}

				logger.LogInformation("Found {NumResults} test results", combinedTestData.Count);
				await UploadTestDataAsync(step.StepId, combinedTestData);
			}
					
			if (DirectoryReference.Exists(logDir))
			{
				Dictionary<FileReference, string> artifactFileToId = new Dictionary<FileReference, string>();
				foreach (FileReference artifactFile in DirectoryReference.EnumerateFiles(logDir, "*", SearchOption.AllDirectories))
				{
					string artifactName = artifactFile.MakeRelativeTo(logDir);

					string? artifactId = await ArtifactUploader.UploadAsync(_rpcConnection, _jobId, _batchId, step.StepId, artifactName, artifactFile, logger, cancellationToken);
					if (artifactId != null)
					{
						artifactFileToId[artifactFile] = artifactId;
					}
				}

				foreach (FileReference reportFile in artifactFileToId.Keys.Where(x => x.HasExtension(".report.json")))
				{
					try
					{
						await CreateReportAsync(step.StepId, reportFile, artifactFileToId, logger);
					}
					catch(Exception ex)
					{
						logger.LogWarning("Unable to upload report: {Message}", ex.Message);
					}
				}
			}

			return exitCode;
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
			await _rpcConnection.InvokeAsync(x => x.CreateReportAsync(request), new RpcContext(), CancellationToken.None);
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
				await _rpcConnection.InvokeAsync(x => UploadTestDataAsync(x, jobStepId, testData), new RpcContext(), CancellationToken.None);
			}
		}

		async Task<bool> UploadTestDataAsync(HordeRpc.HordeRpcClient rpcClient, string jobStepId, IEnumerable<KeyValuePair<string, object>> pairs)
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
}
