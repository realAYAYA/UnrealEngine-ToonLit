// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Configuration;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Commands.Bundles
{
	[Command("bundle", "perforce", "Replicates commits for a particular change of changes from Perforce")]
	class PerforceCommand : Command
	{
		[CommandLine("-Stream=", Required = true)]
		public string StreamId { get; set; } = String.Empty;

		[CommandLine(Required = true)]
		public int Change { get; set; }

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		public PerforceCommand(IConfiguration configuration, ILoggerProvider loggerProvider, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
			_globalConfig = globalConfig;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			PerforceReplicator replicator = serviceProvider.GetRequiredService<PerforceReplicator>();
			IStreamCollection streamCollection = serviceProvider.GetRequiredService<IStreamCollection>();

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(new StreamId(StreamId), out streamConfig))
			{
				throw new FatalErrorException($"Stream '{StreamId}' not found");
			}

			PerforceReplicationOptions options = new PerforceReplicationOptions();
			await replicator.WriteAsync(streamConfig, Change, options, default);

			return 0;
		}
	}
}
