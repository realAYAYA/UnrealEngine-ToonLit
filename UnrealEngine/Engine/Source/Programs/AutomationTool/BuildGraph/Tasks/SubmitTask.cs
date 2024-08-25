// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the submit task
	/// </summary>
	public class SubmitTaskParameters
	{
		/// <summary>
		/// The description for the submitted changelist.
		/// </summary>
		[TaskParameter]
		public string Description;

		/// <summary>
		/// The files to submit.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The Perforce file type for the submitted files (for example, binary+FS32).
		/// </summary>
		[TaskParameter(Optional = true)]
		public string FileType;

		/// <summary>
		/// The workspace name. If specified, a new workspace will be created using the given stream and root directory to submit the files. If not, the current workspace will be used.
		/// </summary>
		[TaskParameter(Optional=true)]
		public string Workspace;

		/// <summary>
		/// The stream for the workspace -- defaults to the current stream. Ignored unless the Workspace attribute is also specified.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Stream;

		/// <summary>
		/// Branch for the workspace (legacy P4 depot path). May not be used in conjunction with Stream.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Branch;

		/// <summary>
		/// Root directory for the stream. If not specified, defaults to the current root directory.
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference RootDir;

		/// <summary>
		/// Whether to revert unchanged files before attempting to submit.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool RevertUnchanged;

		/// <summary>
		/// Force the submit to happen -- even if a resolve is needed (always accept current version).
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Force;

		/// <summary>
		/// Allow verbose P4 output (spew).
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool P4Verbose;
	}

	/// <summary>
	/// Creates a new changelist and submits a set of files to a Perforce stream.
	/// </summary>
	[TaskElement("Submit", typeof(SubmitTaskParameters))]
	public class SubmitTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		SubmitTaskParameters Parameters;

		/// <summary>
		/// Construct a version task
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public SubmitTask(SubmitTaskParameters InParameters)
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
			HashSet<FileReference> Files = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet);
			if (Files.Count == 0)
			{
				Logger.LogInformation("No files to submit.");
			}
			else if (!CommandUtils.AllowSubmit)
			{
				Logger.LogWarning("Submitting to Perforce is disabled by default. Run with the -submit argument to allow.");
			}
			else
			{
				try
				{
					// Get the connection that we're going to submit with
					P4Connection SubmitP4 = CommandUtils.P4;
					if (Parameters.Workspace != null)
					{
						// Create a brand new workspace
						P4ClientInfo Client = new P4ClientInfo();
						Client.Owner = CommandUtils.P4Env.User;
						Client.Host = Unreal.MachineName;
						Client.RootPath = Parameters.RootDir.FullName ?? Unreal.RootDirectory.FullName;
						Client.Name = $"{Parameters.Workspace}_{Regex.Replace(Client.Host, "[^a-zA-Z0-9]", "-")}_{ContentHash.MD5((CommandUtils.P4Env.ServerAndPort ?? "").ToUpperInvariant())}";
						Client.Options = P4ClientOption.NoAllWrite | P4ClientOption.Clobber | P4ClientOption.NoCompress | P4ClientOption.Unlocked | P4ClientOption.NoModTime | P4ClientOption.RmDir;
						Client.LineEnd = P4LineEnd.Local;
						if (!String.IsNullOrEmpty(Parameters.Branch))
						{
							Client.View.Add(new KeyValuePair<string, string>($"{Parameters.Branch}/...", $"/..."));
						}
						else
						{
							Client.Stream = Parameters.Stream ?? CommandUtils.P4Env.Branch;
						}
						CommandUtils.P4.CreateClient(Client, AllowSpew: Parameters.P4Verbose);

						// Create a new connection for it
						SubmitP4 = new P4Connection(Client.Owner, Client.Name);
					}

					// Get the latest version of it
					int NewCL = SubmitP4.CreateChange(Description: Parameters.Description.Replace("\\n", "\n"));
					foreach(FileReference File in Files)
					{
						SubmitP4.Revert(String.Format("-k \"{0}\"", File.FullName), AllowSpew: Parameters.P4Verbose);
						SubmitP4.Sync(String.Format("-k \"{0}\"", File.FullName), AllowSpew: Parameters.P4Verbose);
						SubmitP4.Add(NewCL, String.Format("\"{0}\"", File.FullName));
						SubmitP4.Edit(NewCL, String.Format("\"{0}\"", File.FullName), AllowSpew: Parameters.P4Verbose);
						if (Parameters.FileType != null)
						{
							SubmitP4.P4(String.Format("reopen -t \"{0}\" \"{1}\"", Parameters.FileType, File.FullName), AllowSpew: Parameters.P4Verbose);
						}
					}

					// Revert any unchanged files
					if(Parameters.RevertUnchanged)
					{
						SubmitP4.RevertUnchanged(NewCL);
						if(SubmitP4.TryDeleteEmptyChange(NewCL))
						{
							Logger.LogInformation("No files to submit; ignored.");
							return Task.CompletedTask;
						}
					}

					// Submit it
					int SubmittedCL;
					SubmitP4.Submit(NewCL, out SubmittedCL, Force: Parameters.Force);
					if (SubmittedCL <= 0)
					{
						throw new AutomationException("Submit failed.");
					}

					Logger.LogInformation("Submitted in changelist {SubmittedCL}", SubmittedCL);
				}
				catch (P4Exception Ex)
				{
					Logger.LogError(KnownLogEvents.Systemic_Perforce, "{Message}", Ex.Message);
					throw new AutomationException(Ex.ErrorCode, Ex, "{0}", Ex.Message) { OutputFormat = AutomationExceptionOutputFormat.Silent };
				}
			}
			return Task.CompletedTask;
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
			return FindTagNamesFromFilespec(Parameters.Files);
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
