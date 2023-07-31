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
	class CherryPickCommand : Command
	{
		public override string Description => "Cherry-picks a changelist into the current stream";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Cherry-Pick", "%S") { ShowConsole = true };

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

			using PerforceConnection Perforce = new PerforceConnection(null, null, null, Logger);
			int NewChange = await MergeAsync(Perforce, Change, Logger);
			return (NewChange > 0) ? 0 : 1;
		}

		public static async Task<int> MergeAsync(PerforceConnection Perforce, int Change, ILogger Logger)
		{
			InfoRecord Info = await Perforce.GetInfoAsync(InfoOptions.None, CancellationToken.None);
			if (Info.ClientStream == null)
			{
				Logger.LogError("Not currently in a stream workspace.");
				return -1;
			}

			StreamRecord TargetStream = await Perforce.GetStreamAsync(Info.ClientStream, false, CancellationToken.None);
			while (TargetStream.Type == "virtual" && TargetStream.Parent != null)
			{
				TargetStream = await Perforce.GetStreamAsync(TargetStream.Parent, false, CancellationToken.None);
			}

			DescribeRecord ExistingChangeRecord = await Perforce.DescribeAsync(Change, CancellationToken.None);
			if (ExistingChangeRecord.Files.Count == 0)
			{
				Logger.LogError("No files in selected changelist");
				return -1;
			}

			string SourceFileSpec = Regex.Replace(ExistingChangeRecord.Files[0].DepotFile, @"^(//[^/]+/[^/]+)/.*$", $"$1/...@={Change}");
			string TargetFileSpec = $"{TargetStream.Stream}/...";
			Logger.LogInformation("Merging from {SourceSpec} to {TargetSpec}", SourceFileSpec, TargetFileSpec);


			ChangeRecord NewChangeRecord = new ChangeRecord();
			NewChangeRecord.User = Info.UserName;
			NewChangeRecord.Client = Info.ClientName;
			NewChangeRecord.Description = $"{ExistingChangeRecord.Description.TrimEnd()}\n#p4v-cherrypick {Change}";
			NewChangeRecord = await Perforce.CreateChangeAsync(NewChangeRecord, CancellationToken.None);
			Logger.LogInformation("Created pending changelist {Change}", NewChangeRecord.Number);

			await Perforce.MergeAsync(MergeOptions.None, NewChangeRecord.Number, -1, SourceFileSpec, TargetFileSpec, CancellationToken.None);
			Logger.LogInformation("Merged files into changelist {Change}", NewChangeRecord.Number);

			PerforceResponseList<ResolveRecord> ResolveRecords = await Perforce.TryResolveAsync(NewChangeRecord.Number, ResolveOptions.Automatic, FileSpecList.Any, CancellationToken.None);
			if (!ResolveRecords.Succeeded)
			{
				Logger.LogError("Unable to resolve files. Please resolve manually.");
				return -1;
			}

			return NewChangeRecord.Number;
		}
	}
}
