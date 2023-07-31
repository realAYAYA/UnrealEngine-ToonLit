// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class DescribeCommand : Command
	{
		public override string Description => "Prints out a description of the given changelist";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Describe", "%c") { ShowConsole = true, RefreshUI = false};

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

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);
			DescribeRecord DescribeRecord = await Perforce.DescribeAsync(Change, CancellationToken.None);

			foreach (string Line in DescribeRecord.Description.Split('\n'))
			{
				Console.WriteLine(Line);
			}

			return 0;
		}
	}
}
