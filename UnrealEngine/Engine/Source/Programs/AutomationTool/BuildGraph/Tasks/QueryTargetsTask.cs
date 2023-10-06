// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using AutomationTool.Tasks;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildBase;

#nullable enable

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters to query for all the targets in a project
	/// </summary>
	public class QueryTargetsTaskParameters
	{
		/// <summary>
		/// Path to the project file to query
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference? ProjectFile;

		/// <summary>
		/// Path to the output file to receive information about the targets
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference? OutputFile = null;

		/// <summary>
		/// Write out all targets, even if a default is specified in the BuildSettings section of the Default*.ini files. 
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool IncludeAllTargets = false;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string? Tag;
	}

	/// <summary>
	/// Runs UBT to query all the targets for a particular project
	/// </summary>
	[TaskElement("QueryTargets", typeof(QueryTargetsTaskParameters))]
	public class QueryTargetsTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		QueryTargetsTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public QueryTargetsTask(QueryTargetsTaskParameters InParameters)
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
			// Get the output file
			FileReference? OutputFile = Parameters.OutputFile;
			if (OutputFile == null)
			{
				if (Parameters.ProjectFile == null)
				{
					OutputFile = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "TargetInfo.json");
				}
				else
				{
					OutputFile = FileReference.Combine(Parameters.ProjectFile.Directory, "Intermediate", "TargetInfo.json");
				}
			}
			FileUtils.ForceDeleteFile(OutputFile);

			// Run UBT to generate the target info
			List<string> Arguments = new List<string> { "-Mode=QueryTargets" };
			if (Parameters.ProjectFile != null)
			{
				Arguments.Add($"-Project={Parameters.ProjectFile}");
			}
			if (Parameters.IncludeAllTargets)
			{
				Arguments.Add("-IncludeAllTargets");
			}
			CommandUtils.RunUBT(CommandUtils.CmdEnv, Unreal.UnrealBuildToolDllPath, CommandLineArguments.Join(Arguments));

			// Check the output file exists
			if (!FileReference.Exists(OutputFile))
			{
				throw new BuildException($"Missing {OutputFile}");
			}

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).Add(OutputFile);
			}

			// Add the target files to the set of build products
			BuildProducts.Add(OutputFile);
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
			return Enumerable.Empty<string>();
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
