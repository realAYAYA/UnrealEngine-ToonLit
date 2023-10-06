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
	/// Parameters for an AWS CLI task
	/// </summary>
	public class AwsAssumeRoleTaskParameters
	{
		/// <summary>
		/// Role to assume
		/// </summary>
		[TaskParameter]
		public string Arn;

		/// <summary>
		/// Name of this session
		/// </summary>
		[TaskParameter]
		public string Session;

		/// <summary>
		/// Duration of the token in seconds
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Duration = 1000;

		/// <summary>
		/// Environment variables
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Environment;

		/// <summary>
		/// File to read environment variables from
		/// </summary>
		[TaskParameter(Optional = true)]
		public string EnvironmentFile;

		/// <summary>
		/// Output file for the new environment
		/// </summary>
		[TaskParameter]
		public string OutputFile;
	}

	/// <summary>
	/// Assumes an AWS role.
	/// </summary>
	[TaskElement("Aws-AssumeRole", typeof(AwsAssumeRoleTaskParameters))]
	public class AwsAssumeRoleTask : SpawnTaskBase
	{
		class AwsSettings
		{
			public AwsCredentials Credentials { get; set; }
		}

		class AwsCredentials
		{
			public string AccessKeyId { get; set; }
			public string SecretAccessKey { get; set; }
			public string SessionToken { get; set; }
		}

		/// <summary>
		/// Parameters for this task
		/// </summary>
		AwsAssumeRoleTaskParameters Parameters;

		/// <summary>
		/// Construct an AWS CLI task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public AwsAssumeRoleTask(AwsAssumeRoleTaskParameters InParameters)
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
			StringBuilder Arguments = new StringBuilder("sts assume-role");
			if(Parameters.Arn != null)
			{
				Arguments.Append($" --role-arn {Parameters.Arn}");
			}
			if (Parameters.Session != null)
			{
				Arguments.Append($" --role-session-name {Parameters.Session}");
			}
			Arguments.Append($" --duration-seconds {Parameters.Duration}");

			Dictionary<string, string> Environment = SpawnTaskBase.ParseEnvVars(Parameters.Environment, Parameters.EnvironmentFile);
			IProcessResult Result = await SpawnTaskBase.ExecuteAsync("aws", Arguments.ToString(), EnvVars: Environment, LogOutput: false);

			JsonSerializerOptions Options = new JsonSerializerOptions();
			Options.PropertyNameCaseInsensitive = true;

			AwsSettings Settings = JsonSerializer.Deserialize<AwsSettings>(Result.Output, Options);
			if (Settings.Credentials != null)
			{
				if (Settings.Credentials.AccessKeyId != null)
				{
					Environment["AWS_ACCESS_KEY_ID"] = Settings.Credentials.AccessKeyId;
				}
				if (Settings.Credentials.SecretAccessKey != null)
				{
					Environment["AWS_SECRET_ACCESS_KEY"] = Settings.Credentials.SecretAccessKey;
				}
				if (Settings.Credentials.SessionToken != null)
				{
					Environment["AWS_SESSION_TOKEN"] = Settings.Credentials.SessionToken;
				}
			}

			FileReference OutputFile = ResolveFile(Parameters.OutputFile);
			DirectoryReference.CreateDirectory(OutputFile.Directory);
			FileReference.WriteAllLines(OutputFile, Environment.OrderBy(x => x.Key).Select(x => $"{x.Key}={x.Value}"));
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
