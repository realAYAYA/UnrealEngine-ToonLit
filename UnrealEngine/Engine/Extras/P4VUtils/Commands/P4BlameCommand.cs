// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class P4BlameCommand : Command
	{
		public override string Description => "Prints out who last edited a specific line in a file";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Find last edit by line", "%d") 
		{ 
			ShowConsole = true,
			PromptForArgument = true,
			PromptText = "Please enter the line number:"
		};

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			if (Args.Length < 3)
			{
				Logger.LogError("Missing file name and/or line number");
				return 1;
			}

			string FileName = Args[1];
			int LineNumber = 0;
			if (!int.TryParse(Args[2], out LineNumber) || LineNumber == 0)
			{
				Logger.LogError("Please enter a valid line number");
				return 1;
			}

			Logger.LogInformation("Finding last edit for line {Line} in {File}", LineNumber, FileName);

			using PerforceConnection Perforce = new PerforceConnection(null, null, Logger);

			try
			{
				List<AnnotateRecord> Results = await Perforce.AnnotateAsync(FileName, AnnotateOptions.FollowIntegrations | AnnotateOptions.IgnoreWhiteSpaceChanges | AnnotateOptions.OutputUserAndDate, CancellationToken.None);

				if (LineNumber >= Results.Count)
				{
					Logger.LogError("Please enter a valid line number (this file only has {NumLines} lines in P4)", Results.Count);
					return 1;
				}

				// the first object in the list is a header, so the first line is at index 1
				AnnotateRecord LineResult = Results[LineNumber];
				string EscapedString = Regex.Escape(LineResult.Data);

				Logger.LogInformation(@"Change {Change} by {User} on [{Time}]:'{Description}'", LineResult.LowerCl, LineResult.UserName, LineResult.Time, EscapedString);
			}
			catch (PerforceException Exception)
			{
				Logger.LogError(Exception, "{Message}", Exception.Message);
			}

			return 0;
		}
	}
}
