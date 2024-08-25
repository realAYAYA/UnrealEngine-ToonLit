// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using AutomationTool;
using UnrealBuildBase;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using Microsoft.Extensions.Logging;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a Docker-Build task
	/// </summary>
	public class DockerBuildTaskParameters
	{
		/// <summary>
		/// Base directory for the build
		/// </summary>
		[TaskParameter]
		public string BaseDir;

		/// <summary>
		/// Files to be staged before building the image
		/// </summary>
		[TaskParameter]
		public string Files;

		/// <summary>
		/// Path to the Dockerfile. Uses the root of basedir if not specified.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DockerFile;

		/// <summary>
		/// Path to a .dockerignore. Will be copied to basedir if specified.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DockerIgnoreFile;

		/// <summary>
		/// Use BuildKit in Docker
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool UseBuildKit;

		/// <summary>
		/// Type of progress output (--progress)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string ProgressOutput;

		/// <summary>
		/// Tag for the image
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Tag;

		/// <summary>
		/// Set the target build stage to build (--target)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Target;

		/// <summary>
		/// Custom output exporter. Requires BuildKit (--output)
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Output;

		/// <summary>
		/// Optional arguments
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// List of additional directories to overlay into the staged input files. Allows credentials to be staged, etc...
		/// </summary>
		[TaskParameter(Optional = true)]
		public string OverlayDirs;

		/// <summary>
		/// Environment variables to set
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Environment;

		/// <summary>
		/// File to read environment variables from
		/// </summary>
		[TaskParameter(Optional = true)]
		public string EnvironmentFile;
	}

	/// <summary>
	/// Spawns Docker and waits for it to complete.
	/// </summary>
	[TaskElement("Docker-Build", typeof(DockerBuildTaskParameters))]
	public class DockerBuildTask : SpawnTaskBase
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		DockerBuildTaskParameters Parameters;

		/// <summary>
		/// Construct a Docker task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public DockerBuildTask(DockerBuildTaskParameters InParameters)
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
			Logger.LogInformation("Building Docker image");
			using (LogIndentScope Scope = new LogIndentScope("  "))
			{
				DirectoryReference BaseDir = ResolveDirectory(Parameters.BaseDir);
				List<FileReference> SourceFiles = ResolveFilespec(BaseDir, Parameters.Files, TagNameToFileSet).ToList();
				bool isStagingEnabled = SourceFiles.Count > 0;

				DirectoryReference StagingDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "Docker");
				FileUtils.ForceDeleteDirectoryContents(StagingDir);

				List<FileReference> TargetFiles = SourceFiles.ConvertAll(x => FileReference.Combine(StagingDir, x.MakeRelativeTo(BaseDir)));
				CommandUtils.ThreadedCopyFiles(SourceFiles, BaseDir, StagingDir);

				FileReference DockerIgnoreFileInBaseDir = FileReference.Combine(BaseDir, ".dockerignore");
				FileReference.Delete(DockerIgnoreFileInBaseDir);

				if (!String.IsNullOrEmpty(Parameters.OverlayDirs))
				{
					foreach (string OverlayDir in Parameters.OverlayDirs.Split(';'))
					{
						CommandUtils.ThreadedCopyFiles(ResolveDirectory(OverlayDir), StagingDir);
					}
				}

				StringBuilder Arguments = new StringBuilder("build .");
				if (Parameters.Tag != null)
				{
					Arguments.Append($" -t {Parameters.Tag}");
				}
				if (Parameters.Target != null)
				{
					Arguments.Append($" --target {Parameters.Target}");
				}
				if (Parameters.Output != null)
				{
					if (!Parameters.UseBuildKit)
					{
						throw new AutomationException($"{nameof(Parameters.UseBuildKit)} must be enabled to use '{nameof(Parameters.Output)}' parameter");
					}
					Arguments.Append($" --output {Parameters.Output}");
				}
				if (Parameters.DockerFile != null)
				{
					FileReference DockerFile = ResolveFile(Parameters.DockerFile);
					if (!DockerFile.IsUnderDirectory(BaseDir))
					{
						throw new AutomationException($"Dockerfile '{DockerFile}' is not under base directory ({BaseDir})");
					}
					Arguments.Append($" -f {DockerFile.MakeRelativeTo(BaseDir).QuoteArgument()}");
				}
				if (Parameters.DockerIgnoreFile != null)
				{
					FileReference DockerIgnoreFile = ResolveFile(Parameters.DockerIgnoreFile);
					FileReference.Copy(DockerIgnoreFile, DockerIgnoreFileInBaseDir);
				}
				if (Parameters.ProgressOutput != null)
				{
					Arguments.Append($" --progress={Parameters.ProgressOutput}");
				}
				if (Parameters.Arguments != null)
				{
					Arguments.Append($" {Parameters.Arguments}");
				}

				Dictionary<string, string> EnvVars = ParseEnvVars(Parameters.Environment, Parameters.EnvironmentFile);
				if (Parameters.UseBuildKit)
				{
					EnvVars["DOCKER_BUILDKIT"] = "1";
				}
				
				string WorkingDir = isStagingEnabled ? StagingDir.FullName : BaseDir.FullName;
				string Exe = DockerTask.GetDockerExecutablePath();
				await SpawnTaskBase.ExecuteAsync(Exe, Arguments.ToString(), EnvVars: EnvVars, WorkingDir: WorkingDir, SpewFilterCallback: FilterOutput);
			}
		}

		static Regex FilterOutputPattern = new Regex(@"^#\d+ (?:\d+\.\d+ )?");

		static string FilterOutput(string Line) => FilterOutputPattern.Replace(Line, "");

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
			List<string> TagNames = new List<string>();
			TagNames.AddRange(FindTagNamesFromFilespec(Parameters.DockerFile));
			TagNames.AddRange(FindTagNamesFromFilespec(Parameters.Files));
			return TagNames;
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
