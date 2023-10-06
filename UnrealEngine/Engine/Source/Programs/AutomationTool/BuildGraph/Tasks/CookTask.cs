// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using OpenTracing;
using OpenTracing.Util;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that runs the cooker
	/// </summary>
	public class CookTaskParameters
	{
		/// <summary>
		/// Project file to be cooked.
		/// </summary>
		[TaskParameter]
		public string Project;

		/// <summary>
		/// The cook platform to target (for example, Windows).
		/// </summary>
		[TaskParameter]
		public string Platform;

		/// <summary>
		/// List of maps to be cooked, separated by '+' characters.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Maps;

		/// <summary>
		/// Additional arguments to be passed to the cooker.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Versioned = false;
	
		/// <summary>
		/// Additional arguments to be passed to the cooker.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments = "";


		/// <summary>
		/// Optional path to what editor executable to run for cooking.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string EditorExe = "";

		/// <summary>
		/// Whether to tag the output from the cook. Since cooks produce a lot of files, it can be detrimental to spend time tagging them if we don't need them in a dependent node.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool TagOutput = true;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Cook a selection of maps for a certain platform
	/// </summary>
	[TaskElement("Cook", typeof(CookTaskParameters))]
	public class CookTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		CookTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CookTask(CookTaskParameters InParameters)
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
			// Figure out the project that this target belongs to
			FileReference ProjectFile = null;
			if(Parameters.Project != null)
			{
				ProjectFile = new FileReference(Parameters.Project);
				if(!FileReference.Exists(ProjectFile))
				{
					throw new AutomationException("Missing project file - {0}", ProjectFile.FullName);
				}
			}

			// Execute the cooker
			using (IScope Scope = GlobalTracer.Instance.BuildSpan("Cook").StartActive())
			{
				Scope.Span.SetTag("project", ProjectFile == null ? "UE4" : ProjectFile.GetFileNameWithoutExtension());
				Scope.Span.SetTag("platform", Parameters.Platform);
				string[] Maps = (Parameters.Maps == null)? null : Parameters.Maps.Split(new char[]{ '+' });
				string Arguments = (Parameters.Versioned ? "" : "-Unversioned ") + "-LogCmds=\"LogSavePackage Warning\" " + Parameters.Arguments;
				string EditorExe = (string.IsNullOrWhiteSpace(Parameters.EditorExe) ? "UnrealEditor-Cmd.exe" : Parameters.EditorExe);
				CommandUtils.CookCommandlet(ProjectFile, EditorExe, Maps, null, null, null, Parameters.Platform, Arguments);
			}

			// Find all the cooked files
			List<FileReference> CookedFiles = new List<FileReference>();
			if (Parameters.TagOutput)
			{
				foreach (string Platform in Parameters.Platform.Split('+'))
				{
					DirectoryReference PlatformCookedDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Saved", "Cooked", Platform);
					if (!DirectoryReference.Exists(PlatformCookedDirectory))
					{
						throw new AutomationException("Cook output directory not found ({0})", PlatformCookedDirectory.FullName);
					}
					List<FileReference> PlatformCookedFiles = DirectoryReference.EnumerateFiles(PlatformCookedDirectory, "*", System.IO.SearchOption.AllDirectories).ToList();
					if (PlatformCookedFiles.Count == 0)
					{
						throw new AutomationException("Cooking did not produce any files in {0}", PlatformCookedDirectory.FullName);
					}
					CookedFiles.AddRange(PlatformCookedFiles);

					DirectoryReference PackagingFilesDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Saved", "TmpPackaging", Platform);
					if (DirectoryReference.Exists(PackagingFilesDirectory))
					{
						List<FileReference> PackagingFiles = DirectoryReference.EnumerateFiles(PackagingFilesDirectory, "*", System.IO.SearchOption.AllDirectories).ToList();
						CookedFiles.AddRange(PackagingFiles);
					}
				}
			}

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(CookedFiles);
			}

			// Add them to the set of build products
			BuildProducts.UnionWith(CookedFiles);
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
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
