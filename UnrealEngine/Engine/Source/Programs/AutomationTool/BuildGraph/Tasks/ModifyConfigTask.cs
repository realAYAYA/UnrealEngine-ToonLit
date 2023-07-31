// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a ModifyConfig task
	/// </summary>
	public class ModifyConfigTaskParameters
	{
		/// <summary>
		/// Path to the config file
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string File;

		/// <summary>
		/// The section name to modify
		/// </summary>
		[TaskParameter]
		public string Section;

		/// <summary>
		/// The property name to set
		/// </summary>
		[TaskParameter]
		public string Key;

		/// <summary>
		/// The property value to set
		/// </summary>
		[TaskParameter]
		public string Value;

		/// <summary>
		/// Tag to be applied to the extracted files
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Modifies a config file
	/// </summary>
	[TaskElement("ModifyConfig", typeof(ModifyConfigTaskParameters))]
	public class ModifyConfigTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ModifyConfigTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ModifyConfigTask(ModifyConfigTaskParameters InParameters)
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
			FileReference ConfigFileLocation = ResolveFile(Parameters.File);

			ConfigFile ConfigFile;
			if(FileReference.Exists(ConfigFileLocation))
			{
				ConfigFile = new ConfigFile(ConfigFileLocation);
			}
			else
			{
				ConfigFile = new ConfigFile();
			}

			ConfigFileSection Section = ConfigFile.FindOrAddSection(Parameters.Section);
			Section.Lines.RemoveAll(x => String.Compare(x.Key, Parameters.Key, StringComparison.OrdinalIgnoreCase) == 0);
			Section.Lines.Add(new ConfigLine(ConfigLineAction.Set, Parameters.Key, Parameters.Value));

			FileReference.MakeWriteable(ConfigFileLocation);
			ConfigFile.Write(ConfigFileLocation);

			// Apply the optional tag to the produced archive
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).Add(ConfigFileLocation);
			}

			// Add the archive to the set of build products
			BuildProducts.Add(ConfigFileLocation);
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
			return FindTagNamesFromFilespec(Parameters.File);
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
