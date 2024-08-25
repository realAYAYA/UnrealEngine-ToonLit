// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Replicators;
using EpicGames.Horde.Streams;
using Horde.Server.Configuration;
using Horde.Server.Replicators;
using Horde.Server.Server;
using Horde.Server.Streams;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Test
{
	[Command("test", "replication", "Replicates commits for a particular change of changes from Perforce")]
	class TestReplicationCommand : Command
	{
		[CommandLine("-Stream=", Required = true)]
		public string StreamId { get; set; } = String.Empty;

		[CommandLine("-Replicator=", Required = true)]
		public string ReplicatorId { get; set; } = String.Empty;

		[CommandLine(Required = true)]
		public int Change { get; set; }

		[CommandLine]
		public bool Reset { get; set; }

		[CommandLine]
		public bool Clean { get; set; }

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;

		public TestReplicationCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			ServiceCollection serviceCollection = Startup.CreateServiceCollection(_configuration, _loggerProvider);
			await using ServiceProvider serviceProvider = serviceCollection.BuildServiceProvider();

			ConfigService configService = serviceProvider.GetRequiredService<ConfigService>();
			GlobalConfig globalConfig = await configService.WaitForInitialConfigAsync();

			PerforceReplicator perforceReplicator = serviceProvider.GetRequiredService<PerforceReplicator>();
			IStreamCollection streamCollection = serviceProvider.GetRequiredService<IStreamCollection>();

			StreamConfig? streamConfig;
			if (!globalConfig.TryGetStream(new StreamId(StreamId), out streamConfig))
			{
				throw new FatalErrorException($"Stream '{StreamId}' not found");
			}

			IReplicatorCollection replicatorCollection = serviceProvider.GetRequiredService<IReplicatorCollection>();

			ReplicatorId id = new ReplicatorId(new StreamId(StreamId), new StreamReplicatorId(ReplicatorId));
			IReplicator replicator = await replicatorCollection.GetOrAddAsync(id);

			while (replicator.Pause || replicator.Reset != Reset || replicator.Clean != Clean || replicator.NextChange != Change)
			{
				UpdateReplicatorOptions updateOptions = new UpdateReplicatorOptions { Pause = false, Reset = Reset, Clean = Clean, NextChange = Change };

				IReplicator? nextReplicator = await replicator.TryUpdateAsync(updateOptions);
				if (nextReplicator != null)
				{
					replicator = nextReplicator;
					break;
				}

				replicator = await replicatorCollection.GetOrAddAsync(id);
			}

			PerforceReplicationOptions options = new PerforceReplicationOptions();
			await perforceReplicator.RunOnceAsync(replicator, streamConfig, options, default);

			return 0;
		}
	}
}
