// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildBase;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that strips symbols from a set of files
	/// </summary>
	public class SignTaskParameters
	{
		/// <summary>
		/// List of file specifications separated by semicolons (for example, *.cpp;Engine/.../*.bat), or the name of a tag set.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Optional description for the signed content
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Description;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;

		/// <summary>
		/// If true, the calls to the signing tool will be performed in parallel.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Parallel;
	}

	/// <summary>
	/// Signs a set of executable files with an installed certificate.
	/// </summary>
	[TaskElement("Sign", typeof(SignTaskParameters))]
	public class SignTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SignTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public SignTask(SignTaskParameters InParameters)
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
			// Find the matching files
			FileReference[] Files = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet).OrderBy(x => x.FullName).ToArray();

			// Sign all the files
			CodeSign.SignMultipleIfEXEOrDLL(Job.OwnerCommand, (Files.Select(x => x.FullName).ToList()), Description: Parameters.Description, Parameters.Parallel);

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(Files);
			}

			// Add them to the list of build products
			BuildProducts.UnionWith(Files);
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
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
