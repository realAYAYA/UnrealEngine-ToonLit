// Copyright Epic Games, Inc. All Rights Reserved.

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

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the version task
	/// </summary>
	public class SetVersionTaskParameters
	{
		/// <summary>
		/// The changelist to set in the version files.
		/// </summary>
		[TaskParameter]
		public int Change;

		/// <summary>
		/// The engine compatible changelist to set in the version files.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int CompatibleChange;

		/// <summary>
		/// The branch string.
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// The build version string.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Build;

		/// <summary>
		/// The URL for a running continuous integration job.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BuildURL;

		/// <summary>
		/// Whether to set the IS_LICENSEE_VERSION flag to true.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Licensee;

		/// <summary>
		/// Whether to set the ENGINE_IS_PROMOTED_BUILD flag to true.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Promoted = true;

		/// <summary>
		/// If set, do not write to the files -- just return the version files that would be updated. Useful for local builds.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool SkipWrite;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Updates the local version files (Engine/Source/Runtime/Launch/Resources/Version.h, Engine/Build/Build.version, and Engine/Source/Programs/Shared/Metadata.cs) with the given version information.
	/// </summary>
	[TaskElement("SetVersion", typeof(SetVersionTaskParameters))]
	public class SetVersionTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		SetVersionTaskParameters Parameters;

		/// <summary>
		/// Construct a version task
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public SetVersionTask(SetVersionTaskParameters InParameters)
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
			// Update the version files
			List<FileReference> VersionFiles = UnrealBuild.StaticUpdateVersionFiles(Parameters.Change, Parameters.CompatibleChange, Parameters.Branch, Parameters.Build, Parameters.BuildURL, Parameters.Licensee, Parameters.Promoted, !Parameters.SkipWrite);

			// Apply the optional tag to them
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(VersionFiles);
			}

			// Add them to the list of build products
			BuildProducts.UnionWith(VersionFiles);
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

	/// <summary>
	/// Task wrapper methods
	/// </summary>
	public static partial class StandardTasks
	{
		/// <summary>
		/// Execute a task instance
		/// </summary>
		/// <param name="Task"></param>
		/// <returns></returns>
		public static async Task<FileSet> ExecuteAsync(BgTaskImpl Task)
		{
			HashSet<FileReference> BuildProducts = new HashSet<FileReference>();
			await Task.ExecuteAsync(new JobContext(null!, null!), BuildProducts, new Dictionary<string, HashSet<FileReference>>());
			return FileSet.FromFiles(Unreal.RootDirectory, BuildProducts);
		}

		/// <summary>
		/// Updates the current engine version
		/// </summary>
		public static async Task<FileSet> SetVersionAsync(int Change, string Branch, int? CompatibleChange = null, string Build = null, string BuildURL = null, bool? Licensee = null, bool? Promoted = null, bool? SkipWrite = null)
		{
			SetVersionTaskParameters Parameters = new SetVersionTaskParameters();
			Parameters.Change = Change;
			Parameters.CompatibleChange = CompatibleChange ?? Parameters.CompatibleChange;
			Parameters.Branch = Branch ?? Parameters.Branch;
			Parameters.Build = Build ?? Parameters.Build;
			Parameters.BuildURL = BuildURL ?? Parameters.BuildURL;
			Parameters.Licensee = Licensee ?? Parameters.Licensee;
			Parameters.Promoted = Promoted ?? Parameters.Promoted;
			Parameters.SkipWrite = SkipWrite ?? Parameters.SkipWrite;
			return await ExecuteAsync(new SetVersionTask(Parameters));
		}
	}
}
