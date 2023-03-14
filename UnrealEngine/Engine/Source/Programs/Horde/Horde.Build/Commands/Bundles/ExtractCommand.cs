// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Build.Perforce;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Commands.Bundles
{
	[Command("bundle", "extract", "Extracts data from a bundle to the local hard drive")]
	internal class ExtractCommand : Command
	{
		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("default-ref");

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		protected readonly IConfiguration _configuration;
		protected readonly ILoggerProvider _loggerProvider;

		public ExtractCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			ITreeStore store = serviceProvider.GetRequiredService<ITreeStore<ReplicationService>>();

			ReplicationNode node = await store.ReadTreeAsync<ReplicationNode>(RefName);
			DirectoryNode root = await node.Contents.ExpandAsync();
			await root.CopyToDirectoryAsync(OutputDir.ToDirectoryInfo(), logger, CancellationToken.None);

			return 0;
		}
	}
}
