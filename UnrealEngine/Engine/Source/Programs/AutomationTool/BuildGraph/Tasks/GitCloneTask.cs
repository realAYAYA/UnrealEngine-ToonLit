// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a Git-Checkout task
	/// </summary>
	public class GitCloneTaskParameters
	{
		/// <summary>
		/// Directory for the repository
		/// </summary>
		[TaskParameter]
		public string Dir;

		/// <summary>
		/// The remote to add
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Remote;

		/// <summary>
		/// The branch to check out on the remote
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// Configuration file for the repo. This can be used to set up a remote to be fetched and/or provide credentials.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string ConfigFile;
	}

	/// <summary>
	/// Clones a Git repository into a local path.
	/// </summary>
	[TaskElement("Git-Clone", typeof(GitCloneTaskParameters))]
	public class GitCloneTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		GitCloneTaskParameters Parameters;

		/// <summary>
		/// Construct a Git task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public GitCloneTask(GitCloneTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			FileReference GitExe = CommandUtils.FindToolInPath("git");
			if(GitExe == null)
			{
				throw new AutomationException("Unable to find path to Git. Check you have it installed, and it is on your PATH.");
			}

			DirectoryReference Dir = ResolveDirectory(Parameters.Dir);
			Logger.LogInformation("Cloning Git repository into {Dir}", Parameters.Dir);
			using (LogIndentScope Scope = new LogIndentScope("  "))
			{
				DirectoryReference GitDir = DirectoryReference.Combine(Dir, ".git");
				if (!FileReference.Exists(FileReference.Combine(GitDir, "HEAD")))
				{
					await RunGitAsync(GitExe, $"init \"{Dir}\"", Unreal.RootDirectory);
				}

				if (Parameters.ConfigFile != null)
				{
					CommandUtils.CopyFile(Parameters.ConfigFile, FileReference.Combine(GitDir, "config").FullName);
				}

				if (Parameters.Remote != null)
				{
					await RunGitAsync(GitExe, $"remote add origin {Parameters.Remote}", Dir);
				}

				await RunGitAsync(GitExe, "clean -dxf", Dir);
				await RunGitAsync(GitExe, "fetch --all", Dir);
				await RunGitAsync(GitExe, $"reset --hard {Parameters.Branch}", Dir);
			}
		}

		/// <summary>
		/// Runs a git command
		/// </summary>
		/// <param name="ToolFile"></param>
		/// <param name="Arguments"></param>
		/// <param name="WorkingDir"></param>
		Task RunGitAsync(FileReference ToolFile, string Arguments, DirectoryReference WorkingDir)
		{
			IProcessResult Result = CommandUtils.Run(ToolFile.FullName, Arguments, WorkingDir: WorkingDir.FullName);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException("Git terminated with an exit code indicating an error ({0})", Result.ExitCode);
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
