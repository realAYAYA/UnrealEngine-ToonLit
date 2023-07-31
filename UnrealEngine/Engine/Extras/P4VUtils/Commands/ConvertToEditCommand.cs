// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class ConvertToEditCommand : Command
	{
		public override string Description => "Converts an integration to an edit";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Convert to Edit", "%p") { ShowConsole = true };

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

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);
			if(!await ConvertToEditAsync(Perforce, Change, Debug, Logger))
			{
				return 1;
			}
			return 0;
		}

		public static async Task<bool> ConvertToEditAsync(PerforceConnection Perforce, int Change, bool Debug, ILogger Logger)
		{
			// Get the list of opened files in the CL along with their metadata
			Logger.LogInformation("Getting the list of files in changelist {Change}...", Change);
			List<OpenedRecord> OpenedRecords = await Perforce.OpenedAsync(OpenedOptions.None, Change, null, null, -1, FileSpecList.Any, CancellationToken.None).ToListAsync();
			if (OpenedRecords.Count == 0)
			{
				Logger.LogError("Change has no opened files. Aborting.");
				return false;
			}

			// Each record has {depotFile, clientFile, rev, headRev, action, change, type, user, client}
			Logger.LogInformation("There are {NumFiles} open files in the change.", OpenedRecords.Count);

			// create a mapping of client to local files so we can re-open using local paths (needed for some actions)
			List<WhereRecord> WhereRecords = await Perforce.WhereAsync(OpenedRecords.Select(x => x.DepotFile!).ToArray(), CancellationToken.None).ToListAsync();
			Dictionary<string, string> OpenedClientToLocalMap = WhereRecords.ToDictionary(x => x.ClientFile, x => x.Path, StringComparer.OrdinalIgnoreCase);

			// If any file that needs to be resolved still, we need to abort.
			// We don't want to convert non-resolved files to plain edits,
			// as we need to resolve the merge first!
			List<ResolveRecord> ResolveRecords = await Perforce.ResolveAsync(Change, ResolveOptions.PreviewOnly, FileSpecList.Any, CancellationToken.None);
			if (ResolveRecords.Any())
			{
				Logger.LogError("The following files in the changelist need to be resolved.");
				Logger.LogError("This script cannot continue until all files are resolved:");
				foreach (ResolveRecord ResolveRecord in ResolveRecords)
				{
					Logger.LogError("    {FileToResolve}", ResolveRecord.ClientFile);
				}
				return false;
			}

			// We'll just assume all the files in the changelist are integrated. We'll revert and re-open
			// with the same action/filetype. This should remove the integration records.

			// partition our files into ones we know the actions for, and the ones we don't
			List<OpenedRecord> DeleteActions = new List<OpenedRecord>();
			List<OpenedRecord> AddActions = new List<OpenedRecord>();
			List<OpenedRecord> EditActions = new List<OpenedRecord>();
			List<OpenedRecord> MoveAddActions = new List<OpenedRecord>();
			List<OpenedRecord> MoveDeleteActions = new List<OpenedRecord>();
			List<OpenedRecord> UnknownActions = new List<OpenedRecord>();

			// Multiple action map to the same reopen mapping, so this handles that.
			Dictionary<FileAction, List<OpenedRecord>> ActionMappings = new Dictionary<FileAction, List<OpenedRecord>>()
			{
				[FileAction.Delete] = DeleteActions,
				[FileAction.Branch] = AddActions,
				[FileAction.Add] = AddActions,
				[FileAction.Edit] = EditActions,
				[FileAction.Integrate] = EditActions,
				[FileAction.MoveAdd] = MoveAddActions,
				[FileAction.MoveDelete] = MoveDeleteActions
			};

			// this maps all open records to a reopen action.
			foreach (OpenedRecord OpenedRecord in OpenedRecords)
			{
				List<OpenedRecord>? ActionList;
				if (!ActionMappings.TryGetValue(OpenedRecord.Action, out ActionList))
				{
					ActionList = UnknownActions;
				}
				ActionList.Add(OpenedRecord);
			}

			//  if we have an unknown action, abort with error.
			if (UnknownActions.Count > 0)
			{
				Logger.LogError("These files have unknown actions so cannot be reliably re-opened:");
				foreach (OpenedRecord OpenedRecord in UnknownActions)
				{
					Logger.LogError("  {Action}: {ClientFile}", OpenedRecord.Action.ToString(), OpenedRecord.ClientFile);
				}
				return false;
			}

			// We are good to go, so actually revert the files.
			Logger.LogInformation("Reverting files that will be reopened");
			if (!Debug)
			{
				PerforceResponseList<RevertRecord> Results = await Perforce.TryRevertAsync(-1, null, RevertOptions.KeepWorkspaceFiles, OpenedRecords.Select(x => x.ClientFile!).ToArray(), CancellationToken.None);
				Results.EnsureSuccess();
			}

			// P4 lets you resolve/submit integrations without syncing them to the revision you are resolving against because it's a server operation.
			// if the haveRev doesn't match the rev of the originally opened file, this was the case.
			// so we sync -k because the local file matches the one we want, we just have to tell P4 that so it will let us check it out at that revision.
			// If we simply sync -k to #head we might miss a legitimate submit that happened since our last resolve, and that would stomp that submit.
			List<OpenedRecord> NotSyncedOpenFiles = OpenedRecords.Where(x => x.HaveRevision != x.Revision && x.Action != FileAction.Branch && x.Action != FileAction.Add).ToList();
			if (NotSyncedOpenFiles.Count > 0)
			{
				Logger.LogWarning("The following files were not synced to the revision they were resolved to before. syncing them to the indicated rev first.");
				foreach (OpenedRecord NotSyncedOpenFile in NotSyncedOpenFiles)
				{
					Logger.LogWarning("    {File}#{Revision} (had #{HaveRevision})", NotSyncedOpenFile.ClientFile, NotSyncedOpenFile.Revision, NotSyncedOpenFile.HaveRevision);
				}
				if (!Debug)
				{
					await Perforce.SyncAsync(SyncOptions.KeepWorkspaceFiles, -1, NotSyncedOpenFiles.Select(x => $"{x.ClientFile}#{x.Revision}").ToArray(), CancellationToken.None).ToListAsync();
				}
			}

			// Perform basic actions
			await ReopenFiles("delete", DeleteActions, OpenedClientToLocalMap, x => Perforce.DeleteAsync(Change, DeleteOptions.None, x, CancellationToken.None), Debug, Logger);
			await ReopenFiles("add", AddActions, OpenedClientToLocalMap, x => Perforce.AddAsync(Change, x, CancellationToken.None), Debug, Logger);
			await ReopenFiles("edit", EditActions, OpenedClientToLocalMap, x => Perforce.EditAsync(Change, x, CancellationToken.None), Debug, Logger);

			// Perform move actions
			if (MoveAddActions.Count > 0)
			{
				Logger.LogInformation("Re-opening the following files for move:");

				string[] LocalFiles = MoveAddActions.Select(x => OpenedClientToLocalMap[x.ClientFile!]).ToArray();
				foreach (string LocalFile in LocalFiles)
				{
					Logger.LogInformation("    {LocalFile}", LocalFile);
				}

				// We have to first open the source file for edit (have to use -k because the file has already been moved locally!)
				if (!Debug)
				{
					await Perforce.EditAsync(Change, null, EditOptions.KeepWorkspaceFiles, MoveAddActions.Select(x => x.MovedFile!).ToArray(), CancellationToken.None);

					// then we can open the file for move in the new location (have to use -k because the file has already been moved locally!)
					foreach ((OpenedRecord OpenedRecord, string LocalFile) in MoveAddActions.Zip(LocalFiles))
					{
						await Perforce.MoveAsync(Change, null, MoveOptions.KeepWorkspaceFiles, OpenedRecord.MovedFile!, LocalFile, CancellationToken.None);
					}
				}
			}

			// Get the list of reopened files in the CL to check their filetype
			Logger.LogInformation("Checking the list of files reopened in changelist {Change}...", Change);
			List<OpenedRecord> ReopenedRecords = await Perforce.OpenedAsync(OpenedOptions.None, Change, null, null, -1, FileSpecList.Any, CancellationToken.None).ToListAsync();
			if (ReopenedRecords.Count == 0)
			{
				Logger.LogError("Change has no reopened files. This signifies an error in the script! Aborting...");
				return false;
			}
			if (ReopenedRecords.Count != OpenedRecords.Count)
			{
				Logger.LogError("Change doesn't not have the same number of reopened files ({NumReopened}) as originally ({NumOpened}). This probably signifies an error in the script, and the actions should be reverted! Aborting...", ReopenedRecords.Count, OpenedRecords.Count);
				return false;
			}

			// Ensure the filetypes match and ensure each reopened file was in the original changelist.
			Dictionary<string, OpenedRecord> ReopenedRecordsMap = ReopenedRecords.ToDictionary(x => x.ClientFile!, x => x);
			foreach (OpenedRecord OpenedRecord in OpenedRecords)
			{
				OpenedRecord? ReopenedRecord;
				if (!ReopenedRecordsMap.TryGetValue(OpenedRecord.ClientFile!, out ReopenedRecord))
				{
					Logger.LogError("Could not find original file {ClientFile} in re-opened records. This signifies an error in the script! Aborting...", OpenedRecord.ClientFile);
					return false;
				}
				if (OpenedRecord.Type != ReopenedRecord.Type)
				{
					Logger.LogInformation("Changing filetype of {ClientFile} from {OpenType} to {ReopenType}", OpenedRecord.ClientFile!, ReopenedRecord.Type, OpenedRecord.Type);
					if (!Debug)
					{
						await Perforce.ReopenAsync(Change, OpenedRecord.Type, ReopenedRecord.ClientFile!, CancellationToken.None);
					}
				}
			}

			// Done!
			Logger.LogInformation("Success");
			return true;
		}

		static async Task ReopenFiles(string Operation, List<OpenedRecord> Records, Dictionary<string, string> OpenedClientToLocalMap, Func<string[], Task> ReopenAsync, bool Debug, ILogger Logger)
		{
			if (Records.Count > 0)
			{
				Logger.LogInformation("Re-opening the following files for {Operation}:", Operation);

				string[] LocalFiles = Records.Select(x => PerforceUtils.EscapePath(OpenedClientToLocalMap[x.ClientFile!])).ToArray();
				foreach (string LocalFile in LocalFiles)
				{
					Logger.LogInformation("    {LocalFile}", LocalFile);
				}

				if (!Debug)
				{
					await ReopenAsync(LocalFiles);
				}
			}
		}
	}
}
