// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;
using System.Security.Cryptography;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for <see cref="RandomDataTask"/>.
	/// </summary>
	public class RandomDataTaskParameters
	{
		/// <summary>
		/// The size of each file.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Size = 1024;

		/// <summary>
		/// Number of files to write.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Count = 50;

		/// <summary>
		/// Whether to generate different data for each output file.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Different = true;

		/// <summary>
		/// Output directory
		/// </summary>
		[TaskParameter(Optional = true)]
		public string OutputDir;

		/// <summary>
		/// Optional filter to be applied to the list of input files.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Tag;
	}

	/// <summary>
	/// Creates files containing random data in the specified output directory. Used for generating test data for the temp storage system.
	/// </summary>
	[TaskElement("RandomData", typeof(RandomDataTaskParameters))]
	public class RandomDataTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		RandomDataTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public RandomDataTask(RandomDataTaskParameters InParameters)
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
			DirectoryReference OutputDir = ResolveDirectory(Parameters.OutputDir);

			byte[] buffer = Array.Empty<byte>();
			for (int idx = 0; idx < Parameters.Count; idx++)
			{
				if (idx == 0 || Parameters.Different)
				{
					buffer = RandomNumberGenerator.GetBytes(Parameters.Size);
				}

				FileReference file = FileReference.Combine(OutputDir, $"test-{Parameters.Size}-{idx}.dat");
				await FileReference.WriteAllBytesAsync(file, buffer);
				BuildProducts.Add(file);
			}

			Logger.LogInformation("Created {NumFiles:n0} files of {Size:n0} bytes in {OutputDir} (Different={Different})", Parameters.Count, Parameters.Size, OutputDir, Parameters.Different);

			// Apply the optional output tag to them
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(BuildProducts);
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
