// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	[Command("preflight", CommandCategory.Horde, 1)]
	class PreflightCommand : Command, IDisposable
	{
		static public string StripReviewFyiHashTags(string InString)
		{
			return InString.Replace("#review", "-review", StringComparison.Ordinal).Replace("#codereview", "-codereview", StringComparison.Ordinal).Replace("#fyi", "-fyi", StringComparison.Ordinal);
		}

		internal int Change;
		internal string? ClientName;
		internal PerforceConnection? Perforce;
		internal ClientRecord? Client;
		internal StreamRecord? Stream;
		internal DescribeRecord? DescribeRecord;
		internal List<OpenedRecord>? OpenedRecords;

		public override string Description => "Runs a preflight of the given changelist on Horde";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Preflight...", "%p");

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (!await ParseArguments(Args, ConfigValues, Logger))
			{
				return 1;
			}

			if (!await PrepareChangelist(Logger))
			{
				// User opted out, a clean exit.
				return 0;
			}

			GenerateAndOpenUrl(ConfigValues, Logger);

			return 0;
		}

		internal virtual async Task<bool> ParseArguments(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (Args.Length < 2)
			{
				Logger.LogError("Missing changelist number");
				UserInterface.ShowSimpleDialog(
					"This option requires a changelist number to be provided\r\n" +
					"\r\n" +
					"Please ensure the tool is properly installed and try again \r\n",

					"Invalid Tool Installation?",
					Logger);
				return false;
			}
			else if (!int.TryParse(Args[1], out Change))
			{
				Logger.LogError("'{Argument}' is not a numbered changelist", Args[1]);
				UserInterface.ShowSimpleDialog(
					"This option doesn't currently support using the default changelist\r\n" +
					"\r\n" +
					"Please manually move the files into a non-default \r\n" +
					"changelist on Perforce and try the operation again\r\n",

					"This changelist requires manual fixes",
					Logger);
				return false;
			}

			ClientName = Environment.GetEnvironmentVariable("P4CLIENT");
			Perforce = new PerforceConnection(null, null, ClientName, Logger);

			Client = await Perforce.GetClientAsync(ClientName, CancellationToken.None);
			if (Client.Stream == null)
			{
				Logger.LogError("Not being run from a stream client");
				return false;
			}

			OpenedRecords = await Perforce.OpenedAsync(OpenedOptions.None, Change, null, null, -1, FileSpecList.Any, CancellationToken.None).ToListAsync();

			if (OpenedRecords.Count > 0)
			{
				Logger.LogInformation("Shelving changelist {Change}", Change);
				await Perforce.ShelveAsync(Change, ShelveOptions.Overwrite, new[] { "//..." }, CancellationToken.None);
			}

			List<DescribeRecord> Describe = await Perforce.DescribeAsync(DescribeOptions.Shelved, -1, new[] { Change }, CancellationToken.None);
			if (Describe[0].Files.Count == 0)
			{
				Logger.LogError("No files are shelved in the given changelist");
				return false;
			}

			DescribeRecord = Describe[0];

			Stream = await Perforce.GetStreamAsync(Client.Stream, false, CancellationToken.None);
			while (Stream.Type == "virtual" && Stream.Parent != null)
			{
				Stream = await Perforce.GetStreamAsync(Stream.Parent, false, CancellationToken.None);
			}

			// To support import+ streams get the common prefix and compare it to the Stream,
			// if it doesn't match then we'll pivot to trying to find a stream that does
			string CommonPrefix = DescribeRecord.Files[0].DepotFile;
			if (DescribeRecord.Files.Count > 1)
			{
				// DescribeRecords conveniently come sorted alphabetically by depot file
				// so the common prefix is the common elements of the first and last entry
				string LastFile = DescribeRecord.Files.Last().DepotFile;
				for (int i = 0; i < Math.Min(CommonPrefix.Length, LastFile.Length); i++)
				{
					if (CommonPrefix[i] != LastFile[i])
					{
						CommonPrefix = CommonPrefix.Substring(0, i);
						break;
					}
				}
			}
			// If the common prefix is not the stream name then we'll try to find a valid stream out of the common prefix,
			// but if at any point we have ambiguity we'll just fall back to the original stream again
			if (!CommonPrefix.StartsWith(Stream.Name, StringComparison.Ordinal))
			{
				// Start by determining the common depot
				int CommonStreamNameEnd = CommonPrefix.IndexOf('/', 2);
				if (CommonStreamNameEnd != -1)
				{
					// Get the depot record and extract how many components a stream name has to determine how much
					// of the common prefix to examine for a stream name
					DepotRecord Depot = await Perforce.GetDepotAsync(CommonPrefix.Substring(2, CommonStreamNameEnd - 2));
					int StreamDepth = Depot.GetStreamDepth();
					for (int i = 0; i < StreamDepth && CommonStreamNameEnd != -1; i++)
					{
						CommonStreamNameEnd = CommonPrefix.IndexOf('/', CommonStreamNameEnd + 1);
					}
					if (CommonStreamNameEnd != -1)
					{
						StreamRecord? CommonStream = await Perforce.GetStreamAsync(CommonPrefix.Substring(0,CommonStreamNameEnd), false, CancellationToken.None);
						if (CommonStream != null)
						{
							Stream = CommonStream;
						}
					}
				}
			}

			return true;
		}

		internal virtual async Task<bool> PrepareChangelist(ILogger Logger)
		{
			if (CreateBackupCL())
			{
				// if this CL has files still open within it, create a new CL for those still opened files
				// before sending the original CL to the preflight system - this avoids the problem where Horde
				// cannot take ownership of the original CL and fails to checkin

				if (OpenedRecords!.Count > 0)
				{
					InfoRecord Info = await Perforce!.GetInfoAsync(InfoOptions.None, CancellationToken.None);

					ChangeRecord NewChangeRecord = new ChangeRecord();
					NewChangeRecord.User = Info.UserName;
					NewChangeRecord.Client = Info.ClientName;
					NewChangeRecord.Description = $"{StripReviewFyiHashTags(DescribeRecord!.Description.TrimEnd())}\n#p4v-preflight-copy {Change}";
					NewChangeRecord = await Perforce!.CreateChangeAsync(NewChangeRecord, CancellationToken.None);

					Logger.LogInformation("Created pending changelist {Change}", NewChangeRecord.Number);

					foreach (OpenedRecord OpenedRecord in OpenedRecords)
					{
						if (OpenedRecord.ClientFile != null)
						{
							await Perforce!.ReopenAsync(NewChangeRecord.Number, OpenedRecord.Type, OpenedRecord.ClientFile!, CancellationToken.None);
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

				if (OpenedRecords!.Count > 0 && IsSubmit())
				{
					string Prompt = "Your CL was just shelved however it still has files checked out\r\n" +
							"If the files remain in the CL your preflight will fail to submit\r\n" +
							"\r\n" +
							"Click:\r\n" +
							"[YES] - To revert local files and submit the preflight,\r\n" +
							"[NO] - To start the preflight, and move the files manually\r\n" +
							"[CANCEL] - To cancel the request";
					string Caption = "Your CL will fail to auto-submit unless fixed";

					UserInterface.Button Response = UserInterface.ShowDialog(Prompt, Caption, UserInterface.YesNoCancel, UserInterface.Button.Yes, Logger);



					if (Response == UserInterface.Button.No)
					{
						// do nothing - user has been warned.
					}
					else if (Response == UserInterface.Button.Yes)
					{
						await Perforce!.RevertAsync(Change, null, RevertOptions.None, OpenedRecords.Select(x => x.ClientFile!).ToArray(), CancellationToken.None);
					}
					// any other reply is Cancel (like on Mac, hitting Escape will return a weird string, not Cancel)
					else
					{
						Logger.LogInformation("User Opted to cancel");
						return false;
					}
				}
			}

			return true;
		}

		internal virtual void GenerateAndOpenUrl(IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			string Url = GetUrl(Stream!.Stream, Change, ConfigValues);
			Logger.LogInformation("Opening {Url}", Url);
			ProcessUtils.OpenInNewProcess(Url);
		}

		public virtual bool CreateBackupCL()
		{
			return false;
		}

		public virtual string GetUrl(string Stream, int Change, IReadOnlyDictionary<string, string> ConfigValues)
		{
			string BaseUrl = PreflightCommand.GetHordeServerAddress(ConfigValues);
			return $"{BaseUrl}/preflight?stream={Stream}&change={Change}";
		}

		public virtual bool IsSubmit() { return false; }

		public static string GetHordeServerAddress(IReadOnlyDictionary<string, string> ConfigValues)
		{
			string? BaseUrl;
			if (!ConfigValues.TryGetValue("HordeServer", out BaseUrl))
			{
				BaseUrl = "https://configure-server-url-in-p4vutils.ini";
			}

			return BaseUrl.TrimEnd('/');
		}

		public void Dispose()
		{
			Perforce?.Dispose();
		}
	}

	[Command("preflightandsubmit", CommandCategory.Horde, 2)]
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

	[Command("movewriteablepreflightandsubmit", CommandCategory.Horde, 3)]
	class MoveWriteableFilesthenPreflightAndSubmitCommand : PreflightAndSubmitCommand
	{
		public override string Description => "Moves the writeable files to a new CL, then runs a preflight of the current changelist on Horde and submits it";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Move writeable files, Preflight and submit", "%p");

		public override bool CreateBackupCL()
		{
			return true;
		}
	}

	[Command("openpreflight", CommandCategory.Browser, 1)]
	class OpenPreflightCommand : Command
	{
		public override string Description => "If the changelist has been tagged with #preflight, open the preflight Horde page in the browser";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Open Preflight in browser...", "$c %c");

		public override async Task<int> Execute(string[] args, IReadOnlyDictionary<string, string> configValues, ILogger logger)
		{
			logger.LogInformation("Parsing args ...");

			// Parse command lines
			if (args.Length < 3)
			{
				logger.LogError("Not enough args for command, tool is now exiting");
				return 1;
			}

			string clientSpec = args[1];

			if (!int.TryParse(args[2], out int changeNumber))
			{
				logger.LogError("'{Argument}' is not a numbered changelist, tool is now exiting", args[2]);
				return 1;
			}

			logger.LogInformation("Connecting to Perforce...");

			// We prefer the native client to avoid the problem where different versions of p4.exe expect
			// or return records with different formatting to each other.
			PerforceSettings settings = new PerforceSettings(PerforceSettings.Default) { PreferNativeClient = true, ClientName = clientSpec };

			using IPerforceConnection perforceConnection = await PerforceConnection.CreateAsync(settings, logger);
			if (perforceConnection == null)
			{
				logger.LogError("Failed to connect to Perforce, tool is now exiting");
				return 1;
			}

			DescribeRecord description = await perforceConnection.DescribeAsync(changeNumber);

			MatchCollection matches = Regex.Matches(description.Description, @"#preflight ([0-9a-fA-F]{24})$", RegexOptions.Multiline);
			if (matches.Count == 0)
			{
				string message = $"Description for {changeNumber} does not contain any valid preflight tags";

				logger.LogInformation("{Message}", message);
				UserInterface.ShowSimpleDialog(message, "No Preflights Found", logger);

				return 0;
			}

			logger.LogInformation("Found {Count} preflight tag(s)", matches.Count);

			foreach (Match? match in matches)
			{
				// Fairly sure that match will not be null
				string preflightURL = GetUrl(match!.Groups[1].Value, configValues);

				logger.LogInformation("Opening URL '{URL}' in browser", preflightURL);
				ProcessUtils.OpenInNewProcess(preflightURL);
			}

			return 0;
		}

		private static string GetUrl(string preflightId, IReadOnlyDictionary<string, string> ConfigValues)
		{
			string BaseUrl = PreflightCommand.GetHordeServerAddress(ConfigValues);
			return $"{BaseUrl}/job/{preflightId}";
		}
	}

	[Command("preflighthordeconfig", CommandCategory.Horde, 4)]
	class PreflightHordeConfigCommand : PreflightCommand
	{
		public override string Description => "If the changelist contains Horde configuration file(s), open Horde in the browser to validate the configuration file(s)";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Horde: Validate Configuration Files...", "%p");

		/// <summary>
		/// Is it a Horde server configuration file, ie: ends with .stream.json, .project.json, or is named globals.json
		/// </summary>
		/// <param name="DepotPath"></param>
		/// <returns></returns>
		static internal bool IsHordeConfigurationFile(string DepotPath)
		{
			return DepotPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase);
		}

		internal override async Task<bool> ParseArguments(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (!await base.ParseArguments(Args, ConfigValues, Logger))
			{
				return false;
			}

			if (!DescribeRecord!.Files.Any(x => IsHordeConfigurationFile(x.DepotFile)))
			{
				Logger.LogError("No Horde Configuration Files");
				Logger.LogError("'{Change}' does not contain Horde Configuration files.", Change);
				UserInterface.ShowSimpleDialog(
					$"The specified changelist, {Change}, does not contain any Horde Configuration files.",
					"Invalid Changelist", Logger);
				return false;
			}

			return true;
		}

		public override string GetUrl(string Stream, int Change, IReadOnlyDictionary<string, string> ConfigValues)
		{
			string BaseUrl = PreflightCommand.GetHordeServerAddress(ConfigValues);
			return $"{BaseUrl}/preflightconfig?shelvedchange={Change}";
		}
	}
}
