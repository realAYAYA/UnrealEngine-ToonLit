// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using EpicGames.BuildGraph;
using AutomationTool.Tasks;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a spawn task
	/// </summary>
	public class SpawnTaskParameters
	{
		/// <summary>
		/// Executable to spawn.
		/// </summary>
		[TaskParameter]
		public string Exe;

		/// <summary>
		/// Arguments for the newly created process.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Working directory for spawning the new task
		/// </summary>
		[TaskParameter(Optional = true)]
		public string WorkingDir;

		/// <summary>
		/// Environment variables to set
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Environment;

		/// <summary>
		/// File to read environment from
		/// </summary>
		[TaskParameter(Optional = true)]
		public string EnvironmentFile;

		/// <summary>
		/// Write output to the log
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool LogOutput = true;

		/// <summary>
		/// The minimum exit code, which is treated as an error.
		/// </summary>
		[TaskParameter(Optional = true)]
		public int ErrorLevel = 1;
	}

	/// <summary>
	/// Base class for tasks that run an external tool
	/// </summary>
	public abstract class SpawnTaskBase : BgTaskImpl
	{
		/// <summary>
		/// Execute a command
		/// </summary>
		protected static Task<IProcessResult> ExecuteAsync(string Exe, string Arguments, string WorkingDir = null, Dictionary<string, string> EnvVars = null, bool LogOutput = true, int ErrorLevel = 1, string Input = null, ProcessResult.SpewFilterCallbackType SpewFilterCallback = null)
		{
			if (WorkingDir != null)
			{
				WorkingDir = ResolveDirectory(WorkingDir).FullName;
			}

			CommandUtils.ERunOptions Options = CommandUtils.ERunOptions.Default;
			if (!LogOutput)
			{
				Options |= CommandUtils.ERunOptions.SpewIsVerbose;
			}

			IProcessResult Result = CommandUtils.Run(Exe, Arguments, Env: EnvVars, WorkingDir: WorkingDir, Options: Options, Input: Input, SpewFilterCallback: SpewFilterCallback);
			if (Result.ExitCode < 0 || Result.ExitCode >= ErrorLevel)
			{
				throw new AutomationException("{0} terminated with an exit code indicating an error ({1})", Path.GetFileName(Exe), Result.ExitCode);
			}

			return Task.FromResult(Result);
		}

		/// <summary>
		/// Parses environment from a property and file
		/// </summary>
		/// <param name="Environment"></param>
		/// <param name="EnvironmentFile"></param>
		/// <returns></returns>
		protected static Dictionary<string, string> ParseEnvVars(string Environment, string EnvironmentFile)
		{
			Dictionary<string, string> EnvVars = new Dictionary<string, string>();
			if (Environment != null)
			{
				ParseEnvironment(Environment, ';', EnvVars);
			}
			if (EnvironmentFile != null)
			{
				ParseEnvironment(FileUtils.ReadAllText(ResolveFile(EnvironmentFile)), '\n', EnvVars);
			}
			return EnvVars;
		}

		/// <summary>
		/// Parse environment from a string
		/// </summary>
		/// <param name="Environment"></param>
		/// <param name="Separator"></param>
		/// <param name="EnvVars"></param>
		static void ParseEnvironment(string Environment, char Separator, Dictionary<string, string> EnvVars)
		{
			for (int BaseIdx = 0; BaseIdx < Environment.Length;)
			{
				int EqualsIdx = Environment.IndexOf('=', BaseIdx);
				if (EqualsIdx == -1)
				{
					throw new AutomationException("Missing value in environment variable string '{0}'", Environment);
				}

				int EndIdx = Environment.IndexOf(Separator, EqualsIdx + 1);
				if (EndIdx == -1)
				{
					EndIdx = Environment.Length;
				}

				string Name = Environment.Substring(BaseIdx, EqualsIdx - BaseIdx).Trim();
				string Value = Environment.Substring(EqualsIdx + 1, EndIdx - (EqualsIdx + 1)).Trim();
				EnvVars[Name] = Value;

				BaseIdx = EndIdx + 1;
			}
		}
	}

	/// <summary>
	/// Spawns an external executable and waits for it to complete.
	/// </summary>
	[TaskElement("Spawn", typeof(SpawnTaskParameters))]
	public class SpawnTask : SpawnTaskBase
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SpawnTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public SpawnTask(SpawnTaskParameters InParameters)
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
			await ExecuteAsync(Parameters.Exe, Parameters.Arguments, Parameters.WorkingDir, EnvVars: ParseEnvVars(Parameters.Environment, Parameters.EnvironmentFile), LogOutput: Parameters.LogOutput, ErrorLevel: Parameters.ErrorLevel);
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

	public static partial class StandardTasks
	{
		/// <summary>
		/// Execute an external program
		/// </summary>
		/// <param name="Exe">Executable to spawn.</param>
		/// <param name="Arguments">Arguments for the newly created process.</param>
		/// <param name="WorkingDir">Working directory for spawning the new task.</param>
		/// <param name="Environment">Environment variables to set.</param>
		/// <param name="EnvironmentFile">File to read environment from.</param>
		/// <param name="LogOutput">Write output to the log.</param>
		/// <param name="ErrorLevel">The minimum exit code which is treated as an error.</param>
		public static async Task SpawnAsync(string Exe, string Arguments = null, string WorkingDir = null, string Environment = null, string EnvironmentFile = null, bool? LogOutput = null, int? ErrorLevel = null)
		{
			SpawnTaskParameters Parameters = new SpawnTaskParameters();
			Parameters.Exe = Exe;
			Parameters.Arguments = Arguments;
			Parameters.WorkingDir = WorkingDir;
			Parameters.Environment = Environment;
			Parameters.EnvironmentFile = EnvironmentFile;
			Parameters.LogOutput = LogOutput ?? Parameters.LogOutput;
			Parameters.ErrorLevel = ErrorLevel ?? Parameters.ErrorLevel;

			await ExecuteAsync(new SpawnTask(Parameters));
		}
	}
}
