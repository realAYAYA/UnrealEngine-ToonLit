// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

namespace P4VUtils.Commands
{
	abstract class DataCommandBase : Command
	{

		protected static async Task<int> MakeDataFilesLocalWritable(List<(string, string)> FilesInfo, ILogger Logger)
		{
			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);

			List<string> FilesToMakeLocalWritable = FilesInfo.Where(t => t.Item2.StartsWith("binary", StringComparison.Ordinal)).Select(t => t.Item1).ToList();
			List<RevertRecord> Results = await Perforce.RevertAsync(-1, null, RevertOptions.KeepWorkspaceFiles, FilesToMakeLocalWritable, CancellationToken.None);

			return 0;
		}
	}

	class ConvertCLDataToLocalWritable : DataCommandBase
	{
		public override string Description => "Convert all data files in changelist to local writable";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Convert data to local writable", "%c");

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			int ChangeNumber;
			if (Args.Length < 2)
			{
				Logger.LogError("Missing changelist number");
				return 1;
			}
			else if (!int.TryParse(Args[1], out ChangeNumber))
			{
				Logger.LogError("'{Argument}' is not a numbered changelist", Args[1]);
				return 1;
			}

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);
			List<DescribeRecord> Describe = await Perforce.DescribeAsync(DescribeOptions.Shelved, -1, new int[] { ChangeNumber }, CancellationToken.None);
			List<(string, string)> FilesInfo = Describe[0].Files.Select(d => (d.DepotFile, d.Type)).ToList();

			return await MakeDataFilesLocalWritable(FilesInfo, Logger);
		}
	}

	class ConvertDataToLocalWritable : DataCommandBase
	{
		public override string Description => "Convert all selected data files to local writable";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Convert data to local writable", "%F");

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (Args.Length < 2)
			{
				Logger.LogError("Missing file list");
				return 1;
			}

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);
			List<FStatRecord> StatRecords = await Perforce.FStatAsync(Args.Skip(1).ToList(), CancellationToken.None).ToListAsync();
			List<(string, string)> FilesInfo = StatRecords.Select(r => (r.DepotFile!, r.Type!)).ToList();

			return await MakeDataFilesLocalWritable(FilesInfo, Logger);
		}
	}
}