// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.Xml;
using System.IO;
using EpicGames.Core;
using UnrealBuildBase;
using EpicGames.BuildGraph;
using AutomationTool.Tasks;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	static class StringExtensions
	{
		public static bool CaseInsensitiveContains(this string Text, string Value)
		{
			return System.Globalization.CultureInfo.InvariantCulture.CompareInfo.IndexOf(Text, Value, System.Globalization.CompareOptions.IgnoreCase) >= 0;
		}
	}

	/// <summary>
	/// Parameters for a task that calls another UAT command
	/// </summary>
	public class CommandTaskParameters
	{
		/// <summary>
		/// The command name to execute.
		/// </summary>
		[TaskParameter]
		public string Name;

		/// <summary>
		/// Arguments to be passed to the command.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// If non-null, instructs telemetry from the command to be merged into the telemetry for this UAT instance with the given prefix. May be an empty (non-null) string.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string MergeTelemetryWithPrefix;
	}

	/// <summary>
	/// Invokes an AutomationTool child process to run the given command.
	/// </summary>
	[TaskElement("Command", typeof(CommandTaskParameters))]
	public class CommandTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CommandTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CommandTask(CommandTaskParameters InParameters)
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
			// If we're merging telemetry from the child process, get a temp filename for it
			FileReference TelemetryFile = null;
			if (Parameters.MergeTelemetryWithPrefix != null)
			{
				TelemetryFile = FileReference.Combine(Unreal.RootDirectory, "Engine", "Intermediate", "UAT", "Telemetry.json");
				DirectoryReference.CreateDirectory(TelemetryFile.Directory);
			}

			// Run the command
			StringBuilder CommandLine = new StringBuilder();
			if (Parameters.Arguments == null || (!Parameters.Arguments.CaseInsensitiveContains("-p4") && !Parameters.Arguments.CaseInsensitiveContains("-nop4")))
			{
				CommandLine.AppendFormat("{0} ", CommandUtils.P4Enabled ? "-p4" : "-nop4");
			}
			if (Parameters.Arguments == null || (!Parameters.Arguments.CaseInsensitiveContains("-submit") && !Parameters.Arguments.CaseInsensitiveContains("-nosubmit")))
			{
				if(GlobalCommandLine.Submit)
				{
					CommandLine.Append("-submit ");
				}
				if(GlobalCommandLine.NoSubmit)
				{
					CommandLine.Append("-nosubmit ");
				}
			}
			if (Parameters.Arguments == null || !Parameters.Arguments.CaseInsensitiveContains("-uselocalbuildstorage"))
			{
				if (GlobalCommandLine.UseLocalBuildStorage)
				{
					CommandLine.Append("-uselocalbuildstorage ");
				}
			}

			CommandLine.Append("-NoCompile ");
			CommandLine.Append(Parameters.Name);
			if (!String.IsNullOrEmpty(Parameters.Arguments))
			{
				CommandLine.AppendFormat(" {0}", Parameters.Arguments);
			}
			if (TelemetryFile != null)
			{
				CommandLine.AppendFormat(" -Telemetry={0}", CommandUtils.MakePathSafeToUseWithCommandLine(TelemetryFile.FullName));
			}
			CommandUtils.RunUAT(CommandUtils.CmdEnv, CommandLine.ToString(), Identifier: Parameters.Name);

			// Merge in any new telemetry data that was produced
			if (TelemetryFile != null && FileReference.Exists(TelemetryFile))
			{
				Logger.LogDebug("Merging telemetry from {TelemetryFile}", TelemetryFile);

				TelemetryData NewTelemetry;
				if (TelemetryData.TryRead(TelemetryFile, out NewTelemetry))
				{
					CommandUtils.Telemetry.Merge(Parameters.MergeTelemetryWithPrefix, NewTelemetry);
				}
				else
				{
					Logger.LogWarning("Unable to read UAT telemetry file from {TelemetryFile}", TelemetryFile);
				}
			}
			return Task.CompletedTask;
		}

		/// <summary>
		/// Gets the name of this trace
		/// </summary>
		/// <returns>Name of the trace</returns>
		public override string GetTraceName()
		{
			return String.Format("{0}.{1}", base.GetTraceName(), Parameters.Name.ToLowerInvariant());
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
		/// Runs another UAT command
		/// </summary>
		/// <param name="State">The execution state</param>
		/// <param name="Name">Name of the command to run</param>
		/// <param name="Arguments">Arguments for the command</param>
		/// <param name="MergeTelemetryWithPrefix">If non-null, instructs telemetry from the command to be merged into the telemetry for this UAT instance with the given prefix. May be an empty (non-null) string.</param>
		public static async Task CommandAsync(this BgContext State, string Name, string Arguments = null, string MergeTelemetryWithPrefix = null)
		{
			CommandTaskParameters Parameters = new CommandTaskParameters();
			Parameters.Name = Name;
			Parameters.Arguments = Arguments ?? Parameters.Arguments;
			Parameters.MergeTelemetryWithPrefix = MergeTelemetryWithPrefix ?? Parameters.MergeTelemetryWithPrefix;

			await ExecuteAsync(new CommandTask(Parameters));
		}
	}
}
