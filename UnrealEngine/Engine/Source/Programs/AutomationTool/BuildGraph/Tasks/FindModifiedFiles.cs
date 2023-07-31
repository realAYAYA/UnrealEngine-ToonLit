// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging.Abstractions;
using System;
using System.Collections.Generic;
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
	/// Parameters for a task that compiles a C# project
	/// </summary>
	public class FindModifiedFilesTaskParameters
	{
		/// <summary>
		/// The C# project file to compile. Using semicolons, more than one project file can be specified.
		/// </summary>
		[TaskParameter]
		public string Paths;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Change;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int MinChange;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int MaxChange;

		/// <summary>
		/// The file to write to
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference Output;
	}

	/// <summary>
	/// Compiles C# project files, and their dependencies.
	/// </summary>
	[TaskElement("FindModifiedFiles", typeof(FindModifiedFilesTaskParameters))]
	public class FindModifiedFilesTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		FindModifiedFilesTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public FindModifiedFilesTask(FindModifiedFilesTaskParameters InParameters)
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
			using IPerforceConnection Connection = await PerforceConnection.CreateAsync(CommandUtils.P4Settings, Log.Logger);

			StringBuilder Filter = new StringBuilder($"//{Connection.Settings.ClientName}/...");
			if (Parameters.Change > 0)
			{
				Filter.Append($"@={Parameters.Change}");
			}
			else if (Parameters.MinChange > 0)
			{
				if (Parameters.MaxChange > 0)
				{
					Filter.Append($"@{Parameters.MinChange},{Parameters.MaxChange}");
				}
				else
				{
					Filter.Append($"@>={Parameters.MinChange}");
				}
			}
			else
			{
				throw new AutomationException("Change or MinChange must be specified to FindModifiedFiles task");
			}

			StreamRecord StreamRecord = await Connection.GetStreamAsync(CommandUtils.P4Env.Branch, true);
			PerforceViewMap ViewMap = PerforceViewMap.Parse(StreamRecord.View);

			List<string> LocalFiles = new List<string>();

			List<FilesRecord> Files = await Connection.FilesAsync(FilesOptions.None, Filter.ToString());
			foreach (FilesRecord File in Files)
			{
				string LocalFile;
				if (ViewMap.TryMapFile(File.DepotFile, StringComparison.OrdinalIgnoreCase, out LocalFile))
				{
					LocalFiles.Add(LocalFile);
				}
				else
				{
					Log.TraceInformation("Unable to map {0} to workspace; skipping.", File.DepotFile);
				}
			}

			Log.TraceInformation("Found {0} modified files matching {1}", LocalFiles.Count, Filter.ToString());
			foreach (string LocalFile in LocalFiles)
			{
				Log.TraceInformation("  {0}", LocalFile);
			}

			if (Parameters.Output != null)
			{
				await FileReference.WriteAllLinesAsync(Parameters.Output, LocalFiles);
				Log.TraceInformation("Written {0}", Parameters.Output);
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
			//			return FindTagNamesFromFilespec(Parameters.Project);
			return Enumerable.Empty<string>();
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return Enumerable.Empty<string>();
/*			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				yield return TagName;
			}

			foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
			{
				yield return TagName;
			}*/
		}
	}
}
