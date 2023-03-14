// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Horde.Build.Agents.Leases;
using Horde.Build.Configuration;
using Horde.Build.Jobs.Templates;
using Horde.Build.Perforce;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Commands.Bundles
{
	using StreamId = StringId<IStream>;

	[Command("bundle", "perforce", "Replicates commits for a particular change of changes from Perforce")]
	class PerforceCommand : Command
	{
		[CommandLine("-Stream=", Required = true)]
		public string StreamId { get; set; } = String.Empty;

		[CommandLine(Required = true)]
		public int Change { get; set; }

		[CommandLine]
		public int BaseChange { get; set; }

		[CommandLine]
		public int Count { get; set; } = 1;

		[CommandLine]
		public bool Content { get; set; }

		[CommandLine]
		public bool Compact { get; set; } = true;

		[CommandLine]
		public string Filter { get; set; } = "...";

		[CommandLine]
		public bool RevisionsOnly { get; set; } = false;

		[CommandLine]
		public DirectoryReference? OutputDir { get; set; }

		protected readonly IConfiguration _configuration;
		protected readonly ILoggerProvider _loggerProvider;

		public PerforceCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			CommitService commitService = serviceProvider.GetRequiredService<CommitService>();
			ReplicationService replicationService = serviceProvider.GetRequiredService<ReplicationService>();
			ICommitCollection commitCollection = serviceProvider.GetRequiredService<ICommitCollection>();
			IStreamCollection streamCollection = serviceProvider.GetRequiredService<IStreamCollection>();
			IStorageClient storageClient = serviceProvider.GetRequiredService<IStorageClient>();

			IStream? stream = await streamCollection.GetAsync(new StreamId(StreamId));
			if (stream == null)
			{
				throw new FatalErrorException($"Stream '{stream}' not found");
			}

			Dictionary<IStream, int> streamToFirstChange = new Dictionary<IStream, int>();
			streamToFirstChange[stream] = Change;

			ReplicationNode baseContents;
			if (BaseChange == 0)
			{
				baseContents = new ReplicationNode(new DirectoryNode(DirectoryFlags.WithGitHashes));
			}
			else
			{
				baseContents = await replicationService.ReadCommitTreeAsync(stream, BaseChange, Filter, RevisionsOnly, CancellationToken.None);
			}

			await foreach (NewCommit newCommit in commitService.FindCommitsForClusterAsync(stream.ClusterName, streamToFirstChange).Take(Count))
			{
				string briefSummary = newCommit.Description.Replace('\n', ' ');
				logger.LogInformation("");
				logger.LogInformation("Commit {Change} by {AuthorId}: {Summary}", newCommit.Change, newCommit.AuthorId, briefSummary.Substring(0, Math.Min(50, briefSummary.Length)));
				logger.LogInformation("Base path: {BasePath}", newCommit.BasePath);

				if (Content)
				{
					baseContents = await replicationService.WriteCommitTreeAsync(stream, newCommit.Change, BaseChange, baseContents, Filter, RevisionsOnly, CancellationToken.None);
					BaseChange = newCommit.Change;
				}
			}

			return 0;
		}
	}
}
