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
	abstract class UnshelveCommandBase : Command
	{

		protected static async Task<int> UnshelveFiles(List<(string, string, int)> FilesInfoToUnshelve, bool bMakeDataWritable, int ChangeNumber, int IntoChangeNumber, ILogger Logger)
		{
			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);

			List<string> FileSpecs = FilesInfoToUnshelve.Select(t => t.Item1).ToList();
			List<FStatRecord> StatRecords = await Perforce.FStatAsync(FileSpecs, CancellationToken.None).ToListAsync();
			Dictionary<string, int> CurrentFileRevisions = StatRecords.ToDictionary(r => r.DepotFile!, r => r.HaveRevision);
			Dictionary<string, string> CurrentDepotToClientFile = StatRecords.ToDictionary(r => r.DepotFile!, r => r.ClientFile!);
			List<WhereRecord> WhereRecords = await Perforce.WhereAsync(FileSpecs, CancellationToken.None).ToListAsync();

			// Figure out the local path of the added file with a p4 where command
			foreach (WhereRecord WhereRecord in WhereRecords)
			{
				if(!CurrentDepotToClientFile.TryGetValue(WhereRecord.DepotFile, out string? ClientFile) || ClientFile == null)
				{
					CurrentDepotToClientFile[WhereRecord.DepotFile] = WhereRecord.Path;
				}
			}

			List<string> FilesToUnshelve;
			if (bMakeDataWritable)
			{
				FilesToUnshelve = new List<string>();
				FilesInfoToUnshelve.ForEach(async t =>
				{
					(string DepotFile, string Type, int Revision) = t;
					if (Type.StartsWith("binary", StringComparison.Ordinal))
					{
						if (CurrentDepotToClientFile.TryGetValue(DepotFile, out string? ClientFile) && ClientFile != null)
						{
							PrintRecord PrintRecord = await Perforce.PrintAsync(ClientFile, $"{DepotFile}@={ChangeNumber}", CancellationToken.None);
							File.SetAttributes(ClientFile, ~FileAttributes.ReadOnly & File.GetAttributes(ClientFile));
						}
						else
						{
							Logger.LogError("Unshelve failed: could not map locally depot file:{DepotFile}", DepotFile);
						}

					}
					else
					{
						FilesToUnshelve.Add(DepotFile);
					}
				});
			}
			else
			{
				FilesToUnshelve = FilesInfoToUnshelve.Select(t => t.Item1).ToList();
			}

			List<string> FilesToResolve = new List<string>();
			List<string> FilesToSync = new List<string>();
			FilesInfoToUnshelve.ForEach(f =>
			{
				(string DepotFile, string Type, int Revision) = f;
				int CurrentFileRevision;
				if (CurrentFileRevisions.TryGetValue(DepotFile, out CurrentFileRevision) && Revision < CurrentFileRevision)
				{
					FilesToSync.Add(DepotFile + "#" + CurrentFileRevision);
					FilesToResolve.Add(DepotFile);
				}
			});

			List<UnshelveRecord> UnshelveRecords = await Perforce.UnshelveAsync(ChangeNumber, IntoChangeNumber, null, null, null, UnshelveOptions.None, FilesToUnshelve, CancellationToken.None);
			if (FilesToSync.Count > 0)
			{
				List<SyncRecord> SyncFiles = await Perforce.SyncAsync(FilesToSync, CancellationToken.None).ToListAsync();
				SyncFiles.ForEach(r =>
				{
					Logger.LogInformation("Restore file:{ClientFile} to revision:{Revision}", r.Path, r.Revision);
				});

				List<ResolveRecord> ResolveRecords = await Perforce.ResolveAsync(-1, ResolveOptions.Automatic, FilesToResolve, CancellationToken.None);
			}

			return 0;
		}

		protected static async Task<int> UnshelveChangeList(int ChangeNumber, bool bMakeDataWritable, ILogger Logger)
		{
			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);
			List<DescribeRecord> Describe = await Perforce.DescribeAsync(DescribeOptions.Shelved, -1, new int[] { ChangeNumber }, CancellationToken.None);

			int IntoChangeNumber = -1;
			InfoRecord Info = await Perforce.GetInfoAsync(InfoOptions.None, CancellationToken.None);
			if (Describe[0].Client == Info.ClientName)
			{
				IntoChangeNumber = ChangeNumber;
			}

			if (Describe == null || Describe.Count != 1)
			{
				Logger.LogError("Unable to find changelist {Change}", ChangeNumber);
				return 1;
			}
			if (Describe[0].Files.Count == 0)
			{
				Logger.LogError("No files are shelved in the given changelist{Change}", ChangeNumber);
				return 1;
			}

			List<(string, string, int)> FilesInfoToUnshelve = Describe[0].Files.Select(d => (d.DepotFile, d.Type, d.Revision)).ToList();

			return await UnshelveFiles(FilesInfoToUnshelve, bMakeDataWritable, ChangeNumber, IntoChangeNumber, Logger);
		}
	}

	class UnshelveToCurrentRevision : UnshelveCommandBase
	{
		public override string Description => "Remembers revision all files you are about to unshelve and if the revision is older, will sync to saved revision";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Unshelve to current revision", "%p");

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

			return await UnshelveChangeList(ChangeNumber, false/*bMakeDataWritable*/, Logger);
		}
	}

	class UnshelveMakeDataWritable : UnshelveCommandBase
	{
		public override string Description => "Unshelve changelist but for data will make a writable copy locally";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Unshelve and make data writable", "%p");

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

			return await UnshelveChangeList(ChangeNumber, true/*bMakeDataWritable*/, Logger);
		}
	}
}