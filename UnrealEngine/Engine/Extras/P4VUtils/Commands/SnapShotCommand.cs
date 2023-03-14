// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class SnapshotCommand : Command
	{
		public override string Description => "Creates a shelved copy of the CL";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Create a Snapshot shelf of this CL", "%p") { ShowConsole = false, RefreshUI = true };

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

			string? ClientName = Environment.GetEnvironmentVariable("P4CLIENT");
			using PerforceConnection Perforce = new PerforceConnection(null, null, ClientName, Logger);

			ClientRecord Client = await Perforce.GetClientAsync(ClientName, CancellationToken.None);
			if(Client.Stream == null)
			{
				Logger.LogError("Not being run from a stream client");
				return 1;
			}

			// Create the copy of the CL IFF it has files opened
			List<OpenedRecord> OpenedRecords = await Perforce.OpenedAsync(OpenedOptions.None, Change, null, null, -1, FileSpecList.Any, CancellationToken.None).ToListAsync();

			if (OpenedRecords.Count > 0)
			{
				InfoRecord Info = await Perforce.GetInfoAsync(InfoOptions.None, CancellationToken.None);
				DescribeRecord ExistingChangeRecord = await Perforce.DescribeAsync(Change, CancellationToken.None);

				var DateNow = DateTime.Now.ToString();

				ChangeRecord NewChangeRecord = new ChangeRecord();
				NewChangeRecord.User = Info.UserName;
				NewChangeRecord.Client = Info.ClientName;
				NewChangeRecord.Description = $"{PreflightCommand.StripReviewFyiHashTags(ExistingChangeRecord.Description.TrimEnd())}\n[snapshot CL{Change} - {DateNow}]";
				NewChangeRecord = await Perforce.CreateChangeAsync(NewChangeRecord, CancellationToken.None);

				Logger.LogInformation("Created pending changelist {Change}", NewChangeRecord.Number);

				// Move the files to the new CL
				foreach (OpenedRecord OpenedRecord in OpenedRecords)
				{
					if (OpenedRecord.ClientFile != null)
					{
						await Perforce.ReopenAsync(NewChangeRecord.Number, OpenedRecord.Type, OpenedRecord.ClientFile!, CancellationToken.None);
						Logger.LogInformation("moving opened {File} to CL {CL}", OpenedRecord.ClientFile.ToString(), NewChangeRecord.Number);
					}
				}

				// Shelve that CL
				await Perforce.ShelveAsync(NewChangeRecord.Number, ShelveOptions.Overwrite, new[] { "//..." }, CancellationToken.None);

				// Move the files BACK to the old CL
				foreach (OpenedRecord OpenedRecord in OpenedRecords)
				{
					if (OpenedRecord.ClientFile != null)
					{
						await Perforce.ReopenAsync(Change, OpenedRecord.Type, OpenedRecord.ClientFile!, CancellationToken.None);
						Logger.LogInformation("moving opened {File} to CL {CL}", OpenedRecord.ClientFile.ToString(), Change);
					}
				}
			}
			else
			{
				Logger.LogInformation("No files opened, snapshot not created");
			}
			

			
			return 0;
		}
	}
}
