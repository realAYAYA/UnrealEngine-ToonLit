// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

#pragma warning disable SYSLIB0014

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that notarizes a dmg via the apple notarization process
	/// </summary>
	public class NotarizeTaskParameters
	{
		/// <summary>
		/// Path to the dmg to notarize
		/// </summary>
		[TaskParameter]
		public string DmgPath;

		/// <summary>
		/// primary bundle ID
		/// </summary>
		[TaskParameter]
		public string BundleID;

		/// <summary>
		/// Apple ID Username
		/// </summary>
		[TaskParameter]
		public string UserName;

		/// <summary>
		/// The keychain ID
		/// </summary>
		[TaskParameter]
		public string KeyChainID;

		/// <summary>
		/// When true the notarization ticket will be stapled
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool RequireStapling = false;
	}

	[TaskElement("Notarize", typeof(NotarizeTaskParameters))]
	class NotarizeTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		NotarizeTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public NotarizeTask(NotarizeTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Ensure running on a mac.
			if(BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				throw new AutomationException("Notarization can only be run on a Mac!");
			}

			// Ensure file exists
			FileReference Dmg = new FileReference(Parameters.DmgPath);
			if(!FileReference.Exists(Dmg))
			{
				throw new AutomationException("Couldn't find a file to notarize at {0}", Dmg.FullName);
			}

			int ExitCode = 0;
			Logger.LogInformation("Uploading {Arg0} to the notarization server...", Dmg.FullName);
			
			// The notarytool will timeout after 5 retries or 1 hour. Whichever comes first.
			const int MaxNumRetries = 5;
			const int MaxTimeoutInMilliseconds = 3600000;
			long TimeoutInMilliseconds = MaxTimeoutInMilliseconds;
			string Output = "";

			System.Diagnostics.Stopwatch TimeoutStopwatch = System.Diagnostics.Stopwatch.StartNew();
			
			for (int NumRetries = 0; NumRetries < MaxNumRetries; NumRetries++)
			{
				string CommandLine = string.Format("notarytool submit \"{0}\" --keychain-profile \"{1}\" --wait --timeout \"{2}\"", Dmg.FullName, Parameters.KeyChainID, TimeoutInMilliseconds);
				Output = CommandUtils.RunAndLog("xcrun", CommandLine, out ExitCode);
				
				if (ExitCode == 0)
				{
					break;
				}
				
				if (TimeoutStopwatch.ElapsedMilliseconds >= TimeoutInMilliseconds)
				{
					Logger.LogInformation("notarytool timed out after {TimeoutInMilliseconds}ms.", TimeoutInMilliseconds);
					TimeoutStopwatch.Stop();
				}
				else if (NumRetries < MaxNumRetries)
				{
					Logger.LogInformation("notarytool failed with exit {ExitCode} attempting retry {NumRetries} of {MaxNumRetries}", ExitCode, NumRetries, MaxNumRetries);
					Thread.Sleep(2000);
					TimeoutInMilliseconds = MaxTimeoutInMilliseconds - TimeoutStopwatch.ElapsedMilliseconds;
					continue;
				}


				Logger.LogInformation("Retries have been exhausted");
				throw new AutomationException("notarytool failed with exit {0}", ExitCode);
			}

			// Grab the UUID from the log
			string RequestUUID = null;
			try
			{
				RequestUUID = Regex.Match(Output, "id: ([a-zA-Z0-9]{8}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{12})").Groups[1].Value.Trim();
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Couldn't get UUID from the log output {0}", Output);
			}

			try
			{
				MatchCollection StatusMatches = Regex.Matches(Output, "(?<=status: ).+");
				// The last status update is the right one.
				string Status = StatusMatches[StatusMatches.Count - 1].Value.ToLower();

				if (Status == "accepted")
				{
					if(Parameters.RequireStapling)
					{
						// once we have a log file, print it out, staple, and we're done.
						Logger.LogInformation("{Text}", GetRequestLogs(RequestUUID));
						string CommandLine = string.Format("stapler staple {0}", Dmg.FullName);
						Output = CommandUtils.RunAndLog("xcrun", CommandLine, out ExitCode);
						if (ExitCode != 0)
						{
							throw new AutomationException("stapler failed with exit {0}", ExitCode);
						}
					}
				}
				else
				{
					Logger.LogError("{Text}", GetRequestLogs(RequestUUID));
					throw new AutomationException($"Could not notarize the app. Request status: {0}. See log output above.", Status);
				}
			}
			catch (Exception Ex)
			{
				if (Ex is AutomationException)
				{
					throw;
				}
				else
				{
					throw new AutomationException(Ex, "Querying for the notarization result failed, output: {0}", Output);
				}
			}

			return Task.CompletedTask;
		}

		private string GetRequestLogs(string RequestUUID)
		{
			try
			{
				string LogCommand = string.Format("notarytool log {0} --keychain-profile \"{1}\"", RequestUUID, Parameters.KeyChainID);
				IProcessResult LogResult = CommandUtils.Run("xcrun", LogCommand);

				string ResponseContent = null;
				if (LogResult.bExitCodeSuccess)
				{
					ResponseContent = LogResult.Output;
				}

				return ResponseContent;
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, string.Format("Couldn't complete the request, error: {0}", Ex.Message));
			}
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}

}
