// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Workspace
{
	[Command("Workspace", "Setup", "Creates or updates a client to use a given stream")]
	class SetupCommand : WorkspaceCommand
	{
		[CommandLine("-Client=", Required = true)]
		[Description("Name of the client to create")]
		string ClientName { get; set; } = null!;

		[CommandLine("-Stream=", Required = true)]
		[Description("Name of the stream to configure")]
		string StreamName { get; set; } = null!;

		protected override async Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			using IPerforceConnection perforceClient = await perforce.WithClientAsync(ClientName);
			await repo.SetupAsync(perforceClient, StreamName, CancellationToken.None);
		}
	}
}
