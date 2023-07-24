// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{

	class EdigrateCommand : Command
	{
		public override string Description => "Cherry-picks a change to the current stream as an edit";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Edigrate", "%S") { ShowConsole = true };

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			int Change;
			if (Args.Length < 2)
			{
				Logger.LogError("Missing changelist number");
				return 1;
			}
			else if (!int.TryParse(Args[1], out Change))
			{
				Logger.LogError("'{Argument}' is not a numbered changelist", Args[1]);
				return 1;
			}

			bool Debug = Args.Any(x => x.Equals("-Debug", StringComparison.OrdinalIgnoreCase));

			using PerforceConnection Perforce = new PerforceConnection(null, null, null, Logger);

			int MergedChange = await CherryPickCommand.MergeAsync(Perforce, Change, Logger);
			if (MergedChange <= 0)
			{
				return 1;
			}
			if (!await ConvertToEditCommand.ConvertToEditAsync(Perforce, MergedChange, Debug, Logger))
			{
				return 1;
			}

			return 0;
		}
	}
}
