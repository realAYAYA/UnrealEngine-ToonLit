// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that executes MSBuild
	/// </summary>
	public class MsBuildTaskParameters
	{
        /// <summary>
        /// The C# project file to compile. Using semicolons, more than one project file can be specified.
        /// </summary>
        [TaskParameter]
		public string Project;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Configuration;

		/// <summary>
		/// The platform to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Platform;

		/// <summary>
		/// Additional options to pass to the compiler.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// The MSBuild output verbosity.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Verbosity = "minimal";
	}

	/// <summary>
	/// Executes MsBuild
	/// </summary>
	[TaskElement("MsBuild", typeof(MsBuildTaskParameters))]
	public class MsBuildTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		MsBuildTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public MsBuildTask(MsBuildTaskParameters InParameters)
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
			// Get the project file
			HashSet<FileReference> ProjectFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.Project, TagNameToFileSet);
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				if(!FileReference.Exists(ProjectFile))
				{
					throw new AutomationException("Couldn't find project file '{0}'", ProjectFile.FullName);
				}
			}

			// Build the argument list
			List<string> Arguments = new List<string>();
			if(!String.IsNullOrEmpty(Parameters.Platform))
			{
				Arguments.Add(String.Format("/p:Platform={0}", CommandUtils.MakePathSafeToUseWithCommandLine(Parameters.Platform)));
			}
			if(!String.IsNullOrEmpty(Parameters.Configuration))
			{
				Arguments.Add(String.Format("/p:Configuration={0}", CommandUtils.MakePathSafeToUseWithCommandLine(Parameters.Configuration)));
			}
			if(!String.IsNullOrEmpty(Parameters.Arguments))
			{
				Arguments.Add(Parameters.Arguments);
			}
			if(!String.IsNullOrEmpty(Parameters.Verbosity))
			{
				Arguments.Add(String.Format("/verbosity:{0}", Parameters.Verbosity));
			}
			Arguments.Add("/nologo");

			// Build all the projects
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				CommandUtils.MsBuild(CommandUtils.CmdEnv, ProjectFile.FullName, String.Join(" ", Arguments), null);
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
			return FindTagNamesFromFilespec(Parameters.Project);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return new string[0];
		}
	}	
}
