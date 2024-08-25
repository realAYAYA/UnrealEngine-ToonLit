// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Execution
{
	class LocalExecutor : JobExecutor
	{
		private readonly LocalExecutorSettings _settings;
		private readonly DirectoryReference _localWorkspaceDir;

		public LocalExecutor(JobExecutorOptions options, LocalExecutorSettings settings, ILogger logger)
			: base(options, logger)
		{
			_settings = settings;
			if (settings.WorkspaceDir == null)
			{
				_localWorkspaceDir = FindWorkspaceRoot();
			}
			else
			{
				_localWorkspaceDir = new DirectoryReference(settings.WorkspaceDir);
			}
		}

		static DirectoryReference FindWorkspaceRoot()
		{
			const string HordeSlnRelativePath = "Engine/Source/Programs/Horde/Horde.sln";

			DirectoryReference executableFileDir = new(AppContext.BaseDirectory);
			for (DirectoryReference? directory = executableFileDir; directory != null; directory = directory.ParentDirectory)
			{
				FileReference hordeSln = FileReference.Combine(directory, HordeSlnRelativePath);
				if (FileReference.Exists(hordeSln))
				{
					return directory;
				}
			}

			throw new Exception($"Unable to find workspace root directory (looking for '{HordeSlnRelativePath}' in a parent directory of '{executableFileDir}'");
		}

		protected override Task<bool> SetupAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			return SetupAsync(step, _localWorkspaceDir, null, null, logger, cancellationToken);
		}

		protected override Task<bool> ExecuteAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			if (_settings.RunSteps)
			{
				return ExecuteAsync(step, _localWorkspaceDir, null, null, logger, cancellationToken);
			}
			else
			{
				logger.LogInformation("**** SKIPPING NODE {StepName} ****", step.Name);
				return Task.FromResult(true);
			}
		}
	}

	class LocalExecutorFactory : IJobExecutorFactory
	{
		readonly LocalExecutorSettings _settings;
		readonly ILogger<LocalExecutor> _logger;

		public string Name => "Local";

		public LocalExecutorFactory(IOptions<LocalExecutorSettings> settings, ILogger<LocalExecutor> logger)
		{
			_settings = settings.Value;
			_logger = logger;
		}

		public IJobExecutor CreateExecutor(AgentWorkspace workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options)
		{
			return new LocalExecutor(options, _settings, _logger);
		}
	}
}
