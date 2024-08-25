// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Used to pass information to tasks about the currently running job.
	/// </summary>
	public class JobContext
	{
		/// <summary>
		/// The current node name
		/// </summary>
		public string CurrentNode { get; }

		/// <summary>
		/// The command that is running the current job.
		/// </summary>
		public BuildCommand OwnerCommand { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InOwnerCommand">The command running the current job</param>
		public JobContext(BuildCommand InOwnerCommand) : this("Unknown", InOwnerCommand)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InCurrentNode">The current node being executed</param>
		/// <param name="InOwnerCommand">The command running the current job</param>
		public JobContext(string InCurrentNode, BuildCommand InOwnerCommand)
		{
			CurrentNode = InCurrentNode;
			OwnerCommand = InOwnerCommand;
		}
	}
}
