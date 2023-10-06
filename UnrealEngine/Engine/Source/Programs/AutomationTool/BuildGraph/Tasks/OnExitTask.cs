// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a spawn task
	/// </summary>
	public class OnExitTaskParameters
	{
		/// <summary>
		/// Executable to spawn.
		/// </summary>
		[TaskParameter]
		public string Command = String.Empty;

		/// <summary>
		/// Whether to execute on lease termination
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Lease = false;
	}

	/// <summary>
	/// Spawns an external executable and waits for it to complete.
	/// </summary>
	[TaskElement("OnExit", typeof(OnExitTaskParameters))]
	public class OnExitTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		OnExitTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public OnExitTask(OnExitTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <inheritdoc/>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			string[] commands = Parameters.Command.Split('\n').Select(x => x.Trim()).ToArray();
			await AddCleanupCommandsAsync(commands, Parameters.Lease);
		}

		/// <inheritdoc/>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <inheritdoc/>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <inheritdoc/>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
