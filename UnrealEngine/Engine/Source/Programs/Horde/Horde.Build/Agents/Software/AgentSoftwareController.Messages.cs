// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace Horde.Build.Agents.Software
{
	/// <summary>
	/// Information about an agent software channel
	/// </summary>
	public class GetAgentSoftwareChannelResponse
	{
		/// <summary>
		/// Name of the channel
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Name of the user that last modified this channel
		/// </summary>
		public string? ModifiedBy { get; set; }

		/// <summary>
		/// The modified timestamp
		/// </summary>
		public DateTime ModifiedTime { get; set; }

		/// <summary>
		/// Version number of this software
		/// </summary>
		public string? Version { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel">The channel information</param>
		public GetAgentSoftwareChannelResponse(IAgentSoftwareChannel channel)
		{
			Name = channel.Name.ToString();
			ModifiedBy = channel.ModifiedBy;
			ModifiedTime = channel.ModifiedTime;
			Version = channel.Version.ToString();
		}
	}
}
