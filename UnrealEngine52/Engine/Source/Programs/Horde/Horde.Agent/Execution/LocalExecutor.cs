// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Execution
{
	class LocalExecutor : JobExecutor
	{
		private readonly LocalExecutorSettings _settings;
		private readonly DirectoryReference _localWorkspaceDir;

		public LocalExecutor(ISession session, string jobId, string batchId, string agentTypeName, LocalExecutorSettings settings, IHttpClientFactory httpClientFactory, ILogger logger) 
			: base(session, jobId, batchId, agentTypeName, httpClientFactory, logger)
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

			FileReference executableFile = new FileReference(Assembly.GetExecutingAssembly().Location);
			for (DirectoryReference? directory = executableFile.Directory; directory != null; directory = directory.ParentDirectory)
			{
				FileReference HordeSln = FileReference.Combine(directory, HordeSlnRelativePath);
				if (FileReference.Exists(HordeSln))
				{
					return directory;
				}
			}

			throw new Exception($"Unable to find workspace root directory (looking for '{HordeSlnRelativePath}' in a parent directory of '{executableFile.Directory}'");
		}

		protected override Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			return SetupAsync(step, _localWorkspaceDir, null, logger, cancellationToken);
		}

		protected override Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			if (_settings.RunSteps)
			{
				return ExecuteAsync(step, _localWorkspaceDir, null,  logger, cancellationToken);
			}
			else
			{
				logger.LogInformation("**** SKIPPING NODE {StepName} ****", step.Name);
				return Task.FromResult(true);
			}
		}
	}

	class LocalExecutorFactory : JobExecutorFactory
	{
		readonly LocalExecutorSettings _settings;
		readonly IHttpClientFactory _httpClientFactory;
		readonly ILogger<LocalExecutor> _logger;

		public override string Name => "Local";

		public LocalExecutorFactory(IOptions<LocalExecutorSettings> settings, IHttpClientFactory httpClientFactory, ILogger<LocalExecutor> logger)
		{
			_settings = settings.Value;
			_httpClientFactory = httpClientFactory;
			_logger = logger;
		}

		public override JobExecutor CreateExecutor(ISession session, ExecuteJobTask executeJobTask, BeginBatchResponse beginBatchResponse)
		{
			return new LocalExecutor(session, executeJobTask.JobId, executeJobTask.BatchId, beginBatchResponse.AgentType, _settings, _httpClientFactory, _logger);
		}
	}
}
