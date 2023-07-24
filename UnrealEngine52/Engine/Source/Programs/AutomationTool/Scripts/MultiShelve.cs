// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using Microsoft.Extensions.Logging;
using EpicGames.Core;
using UnrealBuildBase;


namespace BuildScripts.Automation
{
	[RequireP4]
	[DoesNotNeedP4CL]
	[Help("Shelves multiple changelists into one (useful for preflighting multiple changes together")]
	[Help("TargetCL=<changelist>", "If not specified, the script will create a new changelist to shelve into, and write out the CL number")]
	[Help("SourceCLs=<cl,cl,...>", "List of changelists to shelve into the TargetCL")]
	class MultiShelve : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string SourceCLsParam = ParseRequiredStringParam("SourceCLs");
			IEnumerable<int> SourceCLs = SourceCLsParam.Split(",", StringSplitOptions.TrimEntries).Select(x => int.Parse(x));
			int TargetCL = ParseParamInt("TargetCL", -1);
			bool bAllowSpew = ParseParam("AllowSpew");
			bool bCleanTarget = ParseParam("CleanTarget");

			P4Connection P4 = new P4Connection(null, null);


			if (TargetCL == -1)
			{
				TargetCL = P4.CreateChange(Description: $"Shelving changelists " + string.Join(", ", SourceCLs));
				Log.TraceInformation($"Creating target changelist >> {TargetCL} << to shelve into");
				Log.TraceInformation($"");
			}
			// make sure there aren't files already in the target CL
			else
			{
				if (P4.ChangeFiles(TargetCL, out _, AllowSpew: bAllowSpew).Count > 0)
				{
					Log.TraceInformation($"");
					Log.TraceInformation($"Changelist {TargetCL} already contains files. Note that these files will be reshelved for each source CL.");
				}

				if (bCleanTarget)
				{
					Log.TraceInformation($"");
					Log.TraceInformation($"Deleting shelved files in target CL {TargetCL}");
					P4.DeleteShelvedFiles(TargetCL, AllowSpew: bAllowSpew);
				}	
			}

			foreach (int CL in SourceCLs)
			{
				List<string> Files = P4.ChangeFiles(CL, out _, AllowSpew: bAllowSpew);

				Log.TraceInformation($"");
				if (Files.Count == 0)
				{
					Log.TraceInformation($"Skipping CL {CL}, it had no files!");
					continue;
				}
				Log.TraceInformation($"Shelving from {CL}:");


				Log.TraceInformation($"  Moving {Files.Count} files from {CL}");
				P4.Reopen(TargetCL, Files, AllowSpew: bAllowSpew);

				Log.TraceInformation($"  Shelving...");
				P4.ShelveNoRevert(TargetCL, AllowSpew: bAllowSpew);

				Log.TraceInformation($"  Moving back...");
				P4.Reopen(CL, Files, AllowSpew: bAllowSpew);
			}
			Log.TraceInformation($"");
			Log.TraceInformation($"All done!");
			Log.TraceInformation($"");
			Log.TraceInformation($"");
		}
	}
}
