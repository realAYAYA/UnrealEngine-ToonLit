// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using AutomationTool;
using UnrealBuildBase;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a Docker-Compose task
	/// </summary>
	public class DockerComposeDownTaskParameters
	{
		/// <summary>
		/// Path to the docker-compose file
		/// </summary>
		[TaskParameter]
		public string File;

		/// <summary>
		/// Arguments for the command
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;
	}

	/// <summary>
	/// Spawns Docker and waits for it to complete.
	/// </summary>
	[TaskElement("Docker-Compose-Down", typeof(DockerComposeDownTaskParameters))]
	public class DockerComposeDownTask : SpawnTaskBase
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		DockerComposeDownTaskParameters Parameters;

		/// <summary>
		/// Construct a Docker-Compose task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public DockerComposeDownTask(DockerComposeDownTaskParameters InParameters)
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
			StringBuilder Arguments = new StringBuilder("--ansi never ");
			if (!String.IsNullOrEmpty(Parameters.File))
			{
				Arguments.Append($"--file {Parameters.File.QuoteArgument()} ");
			}
			Arguments.Append("down");
			if (!String.IsNullOrEmpty(Parameters.Arguments))
			{
				Arguments.Append($" {Parameters.Arguments}");
			}

			Logger.LogInformation("Running docker compose {Arguments}", Arguments.ToString());
			using (LogIndentScope Scope = new LogIndentScope("  "))
			{
				await SpawnTaskBase.ExecuteAsync("docker-compose", Arguments.ToString());
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
			return Enumerable.Empty<string>();
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return Enumerable.Empty<string>();
		}
	}
}
