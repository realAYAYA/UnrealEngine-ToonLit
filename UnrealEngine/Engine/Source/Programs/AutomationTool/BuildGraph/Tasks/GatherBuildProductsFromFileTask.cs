// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

using EpicGames.Core;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{	
	/// <summary>
	/// Parameters for <see cref="GatherBuildProductsFromFileTask"/>
	/// </summary>
	public class GatherBuildProductsFromFileTaskParameters
	{
		/// <summary>
		/// 
		/// </summary>
		[TaskParameter]
		public string BuildProductsFile;
	}

	[TaskElement("GatherBuildProductsFromFile", typeof(GatherBuildProductsFromFileTaskParameters))]
	class GatherBuildProductsFromFileTask : BgTaskImpl
	{
		public GatherBuildProductsFromFileTask(GatherBuildProductsFromFileTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			List<FileReference> CleanupFiles = new List<FileReference>();

			CommandUtils.LogInformation("Gathering BuildProducts from {0}...", Parameters.BuildProductsFile);

			try
			{
				var FileBuildProducts = File.ReadAllLines(Parameters.BuildProductsFile);
				foreach(var BuildProduct in FileBuildProducts)
				{
					CommandUtils.LogInformation("Adding file to build products: {0}", BuildProduct);
					BuildProducts.Add(new FileReference(BuildProduct));
				}
			}
			catch (Exception Ex)
			{
				CommandUtils.LogInformation("Failed to gather build products: {0}", Ex.Message);
			}

			return Task.CompletedTask;
		}
		
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}
		
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}

		public GatherBuildProductsFromFileTaskParameters Parameters;
	}
}
