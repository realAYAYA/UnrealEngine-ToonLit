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
			CommandUtils.LogInformation("Uploading {0} to the notarization server...", Dmg.FullName);
			string CommandLine = string.Format("altool --notarize-app --primary-bundle-id \"{0}\" --username \"{1}\" --password \"@keychain:{2}\" --file \"{3}\"", Parameters.BundleID, Parameters.UserName, Parameters.KeyChainID, Dmg.FullName);
			string Output = "";
			const int MaxNumRetries = 5;
			
			for(int NumRetries = 0;;NumRetries++)
			{
				Output = CommandUtils.RunAndLog("xcrun", CommandLine, out ExitCode);
				
				if(ExitCode == 0)
				{
					break;
				}
				
				if (NumRetries < MaxNumRetries)
				{
					CommandUtils.LogInformation("--notarize-app failed with exit {0} attempting retry {1} of {2}", ExitCode, NumRetries, MaxNumRetries);
					Thread.Sleep(2000);
					continue;
				}
				
				CommandUtils.LogInformation("Retries have been exhausted");
				throw new AutomationException("--notarize-app failed with exit {0}", ExitCode);
			}

			// Grab the UUID from the log
			string RequestUUID = null;
			try
			{
				RequestUUID = Regex.Match(Output, "RequestUUID = ([a-zA-Z0-9]{8}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{4}-[a-zA-Z0-9]{12})").Groups[1].Value.Trim();
			}
			catch(Exception Ex)
			{
				throw new AutomationException(Ex, "Couldn't get UUID from the log output {0}", Output);
			}

			// Wait 2 minutes for the server to associate this build with the UUID it passes back.
			// Trying instantly just returns back and says it can't find it.
			CommandUtils.LogInformation("Waiting 3 minutes for the UUID to propagate...");
			Thread.Sleep(180000);

			// Repeat for an hour until we get something back.
			try
			{
				int Timeout = 0;
				int WaitTime = 30000;
				int MaxTimeout = 120;
				while (Timeout < MaxTimeout)
				{
					CommandLine = string.Format("altool --notarization-info {0} -u \"{1}\" -p \"@keychain:{2}\"", RequestUUID, Parameters.UserName, Parameters.KeyChainID);
					Output = CommandUtils.RunAndLog("xcrun", CommandLine, out ExitCode);
					if (ExitCode != 0)
					{
						throw new AutomationException("--notarization-info failed with exit {0}", ExitCode);
					}

					Match StatusMatches = (new Regex("(?<=Status: ).+")).Match(Output);
					string Status = StatusMatches.Value.ToLower();
					Match LogFileURLMatches = (new Regex("(?<=LogFileURL: ).+")).Match(Output);
					string LogFileUrl = LogFileURLMatches.Value;

					if (Status == "invalid")
					{
						CommandUtils.LogInformation(GetLogFile(LogFileUrl));
						throw new AutomationException("Could not notarize the app. See log output above.");
					}
					else if (Status == "in progress")
					{
						CommandUtils.LogInformation("Notarization still in progress, waiting 30 seconds...");
						Thread.Sleep(WaitTime);
					}
					else if (Status == "success")
					{
						if (LogFileUrl == "(null)")
						{
							CommandUtils.LogInformation("Notarization success but no log file has been generated, waiting 30 seconds...");
							Thread.Sleep(WaitTime);
						}
						else if(Parameters.RequireStapling)
						{
							// once we have a log file, print it out, staple, and we're done.
							CommandUtils.LogInformation(GetLogFile(LogFileUrl));
							CommandLine = string.Format("stapler staple {0}", Dmg.FullName);
							Output = CommandUtils.RunAndLog("xcrun", CommandLine, out ExitCode);
							if (ExitCode != 0)
							{
								throw new AutomationException("stapler failed with exit {0}", ExitCode);
							}
							break;
						}
						else
						{
							// Success, no need to run the stapler.
							break;
						}
					}
					else
					{
						CommandUtils.LogInformation("Status is? {0}", Status);
						Thread.Sleep(WaitTime);
					}
					Timeout++;
				}
				if(Timeout == MaxTimeout)
				{
					throw new AutomationException("Did not get a response back from the notarization server after an hour. Aborting!");
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
					throw new AutomationException(Ex, "Querying for the notarization progress failed, output: {0}", Output);
				}
			}

			return Task.CompletedTask;
		}

		private string GetLogFile(string Url)
		{
			HttpWebRequest Request = (HttpWebRequest)WebRequest.Create(Url);
			Request.Method = "GET";
			try
			{
				WebResponse Response = Request.GetResponse();
				string ResponseContent = null;
				using (StreamReader ResponseReader = new System.IO.StreamReader(Response.GetResponseStream(), Encoding.Default))
				{
					ResponseContent = ResponseReader.ReadToEnd();
				}
				return ResponseContent;
			}
			catch (WebException Ex)
			{
				if (Ex.Response != null)
				{
					throw new AutomationException(Ex, string.Format("Request returned status: {0}, message: {1}", ((HttpWebResponse)Ex.Response).StatusCode, Ex.Message));
				}
				else
				{
					throw new AutomationException(Ex, string.Format("Request returned message: {0}", Ex.InnerException.Message));
				}
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