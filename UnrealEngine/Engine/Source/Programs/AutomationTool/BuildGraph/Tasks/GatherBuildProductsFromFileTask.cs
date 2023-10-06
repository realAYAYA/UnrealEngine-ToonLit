// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

using EpicGames.Core;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

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

			Logger.LogInformation("Gathering BuildProducts from {Arg0}...", Parameters.BuildProductsFile);

			try
			{
				var FileBuildProducts = File.ReadAllLines(Parameters.BuildProductsFile);
				foreach(var BuildProduct in FileBuildProducts)
				{
					Logger.LogInformation("Adding file to build products: {BuildProduct}", BuildProduct);
					BuildProducts.Add(new FileReference(BuildProduct));
				}
			}
			catch (Exception Ex)
			{
				Logger.LogInformation("Failed to gather build products: {Arg0}", Ex.Message);
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
