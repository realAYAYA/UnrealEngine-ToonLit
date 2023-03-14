// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Xml;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a Docker-Build task
	/// </summary>
	public class DockerPushTaskParameters
	{
		/// <summary>
		/// Repository
		/// </summary>
		[TaskParameter]
		public string Repository;

		/// <summary>
		/// Source image to push
		/// </summary>
		[TaskParameter]
		public string Image;

		/// <summary>
		/// Name of the target image
		/// </summary>
		[TaskParameter(Optional = true)]
		public string TargetImage;

		/// <summary>
		/// Additional environment variables
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Environment;

		/// <summary>
		/// File to read environment from
		/// </summary>
		[TaskParameter(Optional = true)]
		public string EnvironmentFile;

		/// <summary>
		/// Whether to login to AWS ECR
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AwsEcr;
	}

	/// <summary>
	/// Spawns Docker and waits for it to complete.
	/// </summary>
	[TaskElement("Docker-Push", typeof(DockerPushTaskParameters))]
	public class DockerPushTask : SpawnTaskBase
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		DockerPushTaskParameters Parameters;

		/// <summary>
		/// Construct a Docker task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public DockerPushTask(DockerPushTaskParameters InParameters)
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
			Log.TraceInformation("Pushing Docker image");
			using (LogIndentScope Scope = new LogIndentScope("  "))
			{
				Dictionary<string, string> Environment = ParseEnvVars(Parameters.Environment, Parameters.EnvironmentFile);

				if (Parameters.AwsEcr)
				{
					IProcessResult Result = await SpawnTaskBase.ExecuteAsync("aws", "ecr get-login-password", EnvVars: Environment, LogOutput: false);
					await ExecuteAsync("docker", $"login {Parameters.Repository} --username AWS --password-stdin", Input: Result.Output);
				}

				string TargetImage = Parameters.TargetImage ?? Parameters.Image;
				await ExecuteAsync("docker", $"tag {Parameters.Image} {Parameters.Repository}/{TargetImage}", EnvVars: Environment);
				await ExecuteAsync("docker", $"push {Parameters.Repository}/{TargetImage}", EnvVars: Environment);
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
			yield break;
		}
	}
}
