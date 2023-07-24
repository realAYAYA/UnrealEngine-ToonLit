// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class FastReconcileCodeEditsCommand : Command
	{
		
		public override string Description => "Performs a fast reconcile of locally writeable code files into the default CL";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Fast Reconcile Writeable Code Files", "%D") { ShowConsole = true, RefreshUI = true };

		public virtual string[] GetExtensions()
		{
			string[] Extensions =
			{
				".c*",
				".h*",
				".up*",
				".ini"
			};
			return Extensions;
		}

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (Args.Length < 2)
			{
				Logger.LogError("no folders selected");
				return 1;
			}

			string[] Extensions = GetExtensions();

			List<string> ReconcilePaths = new List<string>();

			// %D is 'selected files or folders', ensure we only use folders
			for (int i=1;i<Args.Length;i++)
			{
				Logger.LogInformation("params {Idx} - {Arg}", i, Args[i]);

				if (Args[i].EndsWith("...", StringComparison.Ordinal))
				{
					foreach (string ext in Extensions)
					{
						ReconcilePaths.Add(Args[i] + ext);
					}
				}
				else
				{
					Logger.LogError("   ignoring individial selected file {File}", Args[i]);
				}
			}

			foreach (string Path in ReconcilePaths)
			{
				Logger.LogInformation("paths {Path}", Path);
			}

			string? ClientName = Environment.GetEnvironmentVariable("P4CLIENT");
			using PerforceConnection Perforce = new PerforceConnection(null, null, ClientName, Logger);

			ClientRecord Client = await Perforce.GetClientAsync(ClientName, CancellationToken.None);
			if(Client.Stream == null)
			{
				Logger.LogError("Not being run from a stream client");
				return 1;
			}

			DateTime Start = DateTime.Now;


			ReconcileOptions options = ReconcileOptions.Edit | ReconcileOptions.UseFileModification;

			// Enable this to debug without taking action
			// or potentially add the preview option for the user
			//options |= ReconcileOptions.PreviewOnly;

			List<ReconcileRecord> Results = await Perforce.ReconcileAsync(-1, options, ReconcilePaths.ToArray(), CancellationToken.None);

			int Duration = Convert.ToInt32((DateTime.Now - Start).TotalSeconds);

			foreach (ReconcileRecord res in Results)
			{
				Logger.LogInformation("Marked for Edit {Res}", res);
			}
			
			Logger.LogInformation("Completed in {Duration}s", Duration);

			return 0;
		}
	}

	class FastReconcileAllEditsCommand : FastReconcileCodeEditsCommand
	{
		public override string[] GetExtensions()
		{
			string[] Extensions =
			{
				".*",
			};
			return Extensions;
		}

		public override string Description => "Performs a fast reconcile of locally writeable files into the default CL";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Fast Reconcile Writeable Files", "%D") { ShowConsole = true, RefreshUI = true };
	}
}
