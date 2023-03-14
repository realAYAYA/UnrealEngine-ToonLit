// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using P4VUtils.Perforce;

namespace P4VUtils.Commands
{
	class FindLastEditCommand : Command
	{
		public override string Description => "Prints who last edited this file and in which stream";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Find last edit", "%d") { ShowConsole = true };

		private async Task<int> FindLastEditInFileHistory(PerforceConnection Perforce, string FilePath, ILogger Logger)
		{
			string[] Files = { FilePath };
			List<FileLogRecord> FileLogRecords = await Perforce.FileLogAsync(FileLogOptions.None, Files, CancellationToken.None);

			if (FileLogRecords.Count == 0)
			{
				Logger.LogWarning("Could not find any p4 change record for {File}", FilePath);
				return 1;
			}



			// the first file log record is what we're interested in
			// because we're not following integrations, it should be the only one anyway
			FileLogRecord FileLogRecord = FileLogRecords.First();

			// grab the last revision, check the action, continue unless it's an edit/add
			foreach (RevisionRecord RevisionRecord in FileLogRecord.Revisions)
			{
				// skipping integrations that were ignored
				if (RevisionRecord.Action == FileAction.Integrate && RevisionRecord.Integrations[0].Action == IntegrateAction.Ignored)
				{
					// go to the next revision record in this stream
					continue;
				}

				// is it an 'integration'?
				if (P4ActionGroups.IntegrateActions.Contains(RevisionRecord.Action))
				{
					// where did we integrate from?
					foreach (IntegrationRecord IntegrationRecord in RevisionRecord.Integrations)
					{
						if (P4ActionGroups.IntegrateFromActions.Contains(IntegrationRecord.Action))
						{
							// we need to start the search at the integrated revision # otherwise we might loop forever. 
							string NewFile = IntegrationRecord.OtherFile + "#" + IntegrationRecord.EndRevisionNumber;
							return await FindLastEditInFileHistory(Perforce, NewFile, Logger);
						}
					}
					Logger.LogError("Could not find integrate 'from' record");
					break;
				}
				// is it an 'edit'?
				else if (P4ActionGroups.EditActions.Contains(RevisionRecord.Action))
				{
					// we found the perp so stop here

					string[] SplitPath = FileLogRecord.DepotPath.Split("/", StringSplitOptions.RemoveEmptyEntries);
					string Depot = SplitPath[0];
					string Stream = SplitPath[1];

					Logger.LogInformation("Last edit in //{Depot}/{Stream} by '{User}' in CL '{Change}', revision #{Revision} ({Action})",
						Depot, Stream, RevisionRecord.UserName, RevisionRecord.ChangeNumber, RevisionRecord.RevisionNumber, RevisionRecord.Action.ToString());

					return 0;
				}
			}

			return 1;
		}

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (Args.Length < 2)
			{
				Logger.LogError("Missing file name");
				return 1;
			}

			string FileName = Args[1];

			// %d is 'selected file or folder', picked that over %f so I don't have to run p4 where to 
			// convert workspace path to depot path
			if ( FileName.EndsWith("...", StringComparison.Ordinal) )
			{
				Logger.LogError("It looks like you selected a Folder, please select a File and run the tool again");
				return 1;
			}

			Logger.LogInformation("Inspecting revision records for file '{File}'", FileName);

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);

			int Result = await FindLastEditInFileHistory(Perforce, FileName, Logger);

			if ( Result != 0)
			{
				Logger.LogError("Unexpected error occurred.");
			}
				
			return Result;
		}
	}
}
