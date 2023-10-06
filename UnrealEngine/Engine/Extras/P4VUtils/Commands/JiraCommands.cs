// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Text.RegularExpressions;

namespace P4VUtils.Commands
{

	[Command("openjira", CommandCategory.Browser, 2)]
	class OpenJiraCommand : Command
	{
		public override string Description => "Jira: Open issues in browser...";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Jira: Open issues in browser", "%C");

		public override async Task<int> Execute(string[] args, IReadOnlyDictionary<string, string> configValues, ILogger logger)
		{
			logger.LogInformation("Parsing args ...");

			// Parse command lines
			if (args.Length < 2)
			{
				logger.LogError("Not enough args for command, tool is now exiting");
				return 1;
			}

			if (!int.TryParse(args[1], out int changeNumber))
			{
				logger.LogError("'{Argument}' is not a numbered changelist, tool is now exiting", args[1]);
				return 1;
			}

			logger.LogInformation("Connecting to Perforce...");

			PerforceSettings settings = new PerforceSettings(PerforceSettings.Default);

			using IPerforceConnection perforceConnection = await PerforceConnection.CreateAsync(settings, logger);
			if (perforceConnection == null)
			{
				logger.LogError("Failed to connect to Perforce, tool is now exiting");
				return 1;
			}

			PerforceResponse<DescribeRecord> response = await perforceConnection.TryDescribeAsync(changeNumber);
			if (!response.Succeeded)
			{
				logger.LogError("Failed to find the changelist description for {CLNumber} due to {Error}", changeNumber, response.Error?.ToString() ?? "Unknown Error");
				return 1;
			}

			DescribeRecord description = response.Data;
			MatchCollection matches = Regex.Matches(description.Description, @"(?<=#jira.*?)([\w]+-[\d]+)", RegexOptions.Multiline);

			if(matches.Count == 0)
			{
				string message = $"Description for {changeNumber} does not contain any valid jira issue tags (#jira ...)";

				logger.LogInformation("{Message}", message);
				UserInterface.ShowSimpleDialog(message, "No Jira Issues Found", logger);
				return 1;
			}

			logger.LogInformation("Found {NumMatches} jira issues in the description for CL {CLNumber}", matches.Count, changeNumber);
			string jiraServer = GetJiraServerAddress(configValues);

			foreach (Match? match in matches)
			{
				string jiraUrl = jiraServer + "/" + match!.Groups[1].Value;

				logger.LogInformation("Opening webpage for {URL}", jiraUrl);
				ProcessUtils.OpenInNewProcess(jiraUrl);
			}

			logger.LogInformation("Command completed successfully");

			return 0;
		}

		private static string GetJiraServerAddress(IReadOnlyDictionary<string, string> ConfigValues)
		{
			string? BaseUrl;
			if (!ConfigValues.TryGetValue("JiraServer", out BaseUrl))
			{
				BaseUrl = "https://configure-server-url-in-p4vutils.ini";
			}

			return BaseUrl.TrimEnd('/');
		}
	}
}