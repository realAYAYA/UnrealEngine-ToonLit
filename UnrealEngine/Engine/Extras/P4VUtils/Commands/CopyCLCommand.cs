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
using System.Windows;

namespace P4VUtils.Commands
{
	[Command("copyclnum", CommandCategory.Root, 1)]
	class CopyCLCommand : Command
	{
		public override string Description => "copy CL # only from pending and submitted CLs";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Copy CL #", "%c")
		{
			RefreshUI = false,
			Shortcut = new string("Ctrl+Alt+C")
		};

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

			await UserInterface.CopyTextToClipboard(Change.ToString(), Logger);

			return 0;
		}
	}
}
