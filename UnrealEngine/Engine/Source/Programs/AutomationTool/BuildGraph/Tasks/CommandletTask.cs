// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task which runs a UE commandlet
	/// </summary>
	public class CommandletTaskParameters
	{
		/// <summary>
		/// The commandlet name to execute.
		/// </summary>
		[TaskParameter]
		public string Name;

		/// <summary>
		/// The project to run the editor with.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Project;

		/// <summary>
		/// Arguments to be passed to the commandlet.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// The editor executable to use. Defaults to the development UnrealEditor executable for the current platform.
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference EditorExe;

		/// <summary>
		/// The minimum exit code, which is treated as an error.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int ErrorLevel = 1;
	}

	/// <summary>
	/// Spawns the editor to run a commandlet.
	/// </summary>
	[TaskElement("Commandlet", typeof(CommandletTaskParameters))]
	public class CommandletTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CommandletTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandletTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CommandletTask(CommandletTaskParameters InParameters)
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
			// Get the full path to the project file
			FileReference ProjectFile = null;
			if(!String.IsNullOrEmpty(Parameters.Project))
			{
				if(Parameters.Project.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
				{
					ProjectFile = ResolveFile(Parameters.Project);
				}
				else
				{
					ProjectFile = NativeProjects.EnumerateProjectFiles(Log.Logger).FirstOrDefault(x => x.GetFileNameWithoutExtension().Equals(Parameters.Project, StringComparison.OrdinalIgnoreCase));
				}

				if(ProjectFile == null || !FileReference.Exists(ProjectFile))
				{
					throw new BuildException("Unable to resolve project '{0}'", Parameters.Project);
				}
			}

			// Get the path to the editor, and check it exists
			FileSystemReference EditorExe;
			if(Parameters.EditorExe == null)
			{
				EditorExe = ProjectUtils.GetProjectTarget(ProjectFile, UnrealBuildTool.TargetType.Editor, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development, true);
				if (EditorExe == null)
				{
					EditorExe = new FileReference(HostPlatform.Current.GetUnrealExePath("UnrealEditor-Cmd.exe"));
				}
			}
			else
			{
				EditorExe = Parameters.EditorExe;
			}

			// Run the commandlet
			CommandUtils.RunCommandlet(ProjectFile, EditorExe.FullName, Parameters.Name, Parameters.Arguments, Parameters.ErrorLevel);
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

	/// <summary>
	/// Task wrapper methods
	/// </summary>
	public static partial class StandardTasks
	{
		/// <summary>
		/// Task which runs a UE commandlet
		/// </summary>
		/// <param name="State"></param>
		/// <param name="Name">The commandlet name to execute.</param>
		/// <param name="Project">The project to run the editor with.</param>
		/// <param name="Arguments">Arguments to be passed to the commandlet.</param>
		/// <param name="EditorExe">The editor executable to use. Defaults to the development UnrealEditor executable for the current platform.</param>
		/// <param name="ErrorLevel">The minimum exit code, which is treated as an error.</param>
		public static async Task CommandletAsync(this BgContext State, string Name, FileReference Project = null, string Arguments = null, FileReference EditorExe = null, int ErrorLevel = 1)
		{
			CommandletTaskParameters Parameters = new CommandletTaskParameters();
			Parameters.Name = Name;
			Parameters.Project = Project?.FullName;
			Parameters.Arguments = Arguments;
			Parameters.EditorExe = EditorExe;
			Parameters.ErrorLevel = ErrorLevel;
			await ExecuteAsync(new CommandletTask(Parameters));
		}
	}
}
