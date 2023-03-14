// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Horde.Build.Utilities;

namespace Horde.Build.Agents.Software
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;

	/// <summary>
	/// A software channel
	/// </summary>
	public interface IAgentSoftwareChannel
	{
		/// <summary>
		/// The channel id
		/// </summary>
		public AgentSoftwareChannelName Name { get; set; }

		/// <summary>
		/// Name of the user that made the last modification
		/// </summary>
		public string? ModifiedBy { get; set; }

		/// <summary>
		/// Last modification time
		/// </summary>
		public DateTime ModifiedTime { get; set; }

		/// <summary>
		/// The software version number
		/// </summary>
		public string Version { get; set; }
	}
}
