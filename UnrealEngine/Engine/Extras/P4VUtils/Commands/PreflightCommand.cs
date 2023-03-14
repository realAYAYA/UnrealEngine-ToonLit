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
using System.Windows;
using System.Linq;

namespace P4VUtils.Commands
{
	class PreflightCommand : Command
	{

		static public string StripReviewFyiHashTags(string InString)
		{
			return InString.Replace("#review", "-review", StringComparison.Ordinal).Replace("#codereview", "-codereview", StringComparison.Ordinal).Replace("#fyi", "-fyi", StringComparison.Ordinal);
		}
		public override string Description => "Runs a preflight of the given changelist on Horde";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Preflight...", "%p");

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

			List<OpenedRecord> OpenedRecords = await Perforce.OpenedAsync(OpenedOptions.None, Change, null, null, -1, FileSpecList.Any, CancellationToken.None).ToListAsync();

			if (OpenedRecords.Count > 0)
			{
				Logger.LogInformation("Shelving changelist {Change}", Change);
				await Perforce.ShelveAsync(Change, ShelveOptions.Overwrite, new[] { "//..." }, CancellationToken.None);
			}
				
			List<DescribeRecord> Describe = await Perforce.DescribeAsync(DescribeOptions.Shelved, -1, new[] { Change }, CancellationToken.None);
			if (Describe[0].Files.Count == 0)
			{
				Logger.LogError("No files are shelved in the given changelist");
				return 1;
			}

			StreamRecord Stream = await Perforce.GetStreamAsync(Client.Stream, false, CancellationToken.None);
			while (Stream.Type == "virtual" && Stream.Parent != null)
			{
				Stream = await Perforce.GetStreamAsync(Stream.Parent, false, CancellationToken.None);
			}

			if (CreateBackupCL())
			{
				// if this CL has files still open within it, create a new CL for those still opened files
				// before sending the original CL to the preflight system - this avoids the problem where Horde
				// cannot take ownership of the original CL and fails to checkin

				if (OpenedRecords.Count > 0)
				{
					InfoRecord Info = await Perforce.GetInfoAsync(InfoOptions.None, CancellationToken.None);

					ChangeRecord NewChangeRecord = new ChangeRecord();
					NewChangeRecord.User = Info.UserName;
					NewChangeRecord.Client = Info.ClientName;
					NewChangeRecord.Description = $"{StripReviewFyiHashTags(Describe[0].Description.TrimEnd())}\n#p4v-preflight-copy {Change}";
					NewChangeRecord = await Perforce.CreateChangeAsync(NewChangeRecord, CancellationToken.None);

					Logger.LogInformation("Created pending changelist {Change}", NewChangeRecord.Number);

					foreach (OpenedRecord OpenedRecord in OpenedRecords)
					{
						if (OpenedRecord.ClientFile != null)
						{
							await Perforce.ReopenAsync(NewChangeRecord.Number, OpenedRecord.Type, OpenedRecord.ClientFile!, CancellationToken.None);
							Logger.LogInformation("moving opened {File} to CL {CL}", OpenedRecord.ClientFile.ToString(), NewChangeRecord.Number);
						}
					}
				}
				else
				{
					Logger.LogInformation("No files opened, no copy CL created");
				}
			}
			else
			{
				// if this CL has files still open within it, and this is a submit request - warn the user and provide options

				if (OpenedRecords.Count > 0 && IsSubmit())
				{
					MessageBoxResult result= MessageBox.Show(
						"Your CL was just shelved however it still has files checked out\r\n" +
						"If the files remain in the CL your preflight will fail to submit\r\n" +
						"\r\n" +
						"Click:\r\n" +
						"[YES] - To revert local files and submit the preflight,\r\n" +
						"[NO] - To start the preflight, and move the files manually\r\n" +
						"[CANCEL] - To cancel the request", 

						"Your CL will fail to auto-submit unless fixed", 
						MessageBoxButton.YesNoCancel);

					if (result == MessageBoxResult.Cancel)
					{
						Logger.LogInformation("User Opted to cancel");
						return 0;
					}
					else if(result == MessageBoxResult.Yes)
					{
						await Perforce.RevertAsync(Change, null, RevertOptions.None, OpenedRecords.Select(x => x.ClientFile!).ToArray(), CancellationToken.None);
					}
					else
					{
						// do nothing - user has been warned.
					}
				}
			}


			string Url = GetUrl(Stream.Stream, Change, ConfigValues);
			Logger.LogInformation("Opening {Url}", Url);
			OpenUrl(Url);

			return 0;
		}

		public virtual bool CreateBackupCL()
		{
			return false;
		}

		public virtual string GetUrl(string Stream, int Change, IReadOnlyDictionary<string, string> ConfigValues)
		{
			string? BaseUrl;
			if (!ConfigValues.TryGetValue("HordeServer", out BaseUrl))
			{
				BaseUrl = "https://configure-server-url-in-p4vutils.ini";
			}
			return $"{BaseUrl.TrimEnd('/')}/preflight?stream={Stream}&change={Change}";
		}

		public virtual bool IsSubmit() { return false; }

		static void OpenUrl(string Url)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				Process.Start(new ProcessStartInfo(Url) { UseShellExecute = true });
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				Process.Start("xdg-open", Url);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				Process.Start("open", Url);
			}
		}
	}

	class PreflightAndSubmitCommand : PreflightCommand
	{
		public override string Description => "Runs a preflight of the given changelist on Horde and submits it";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Preflight and submit", "%p");

		public override string GetUrl(string Stream, int Change, IReadOnlyDictionary<string, string> ConfigValues)
		{
			return base.GetUrl(Stream, Change, ConfigValues) + "&defaulttemplate=true&submit=true";
		}

		public override bool IsSubmit() { return true; }
	}

	class MoveWriteableFilesthenPreflightAndSubmitCommand : PreflightAndSubmitCommand
	{
		public override string Description => "Moves the writeable files to a new CL, then runs a preflight of the current changelist on Horde and submits it";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Move writeable files, Preflight and submit", "%p");

		public override bool CreateBackupCL()
		{
			return true;
		}
	}
}
