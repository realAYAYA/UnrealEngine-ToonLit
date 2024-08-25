// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable CA1027 // Use [Flags] attribute.
#pragma warning disable CA1069 // Overlapping constants in enum.

using System;
using EpicGames.Horde.Storage.Bundles;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Version number for the compute protocol
	/// </summary>
	public enum ComputeProtocol
	{
		/// <summary>
		/// No version specified
		/// </summary>
		Unknown,

		/// <summary>
		/// Initial version number
		/// </summary>
		Initial,

		/// <summary>
		/// Constant for the latest protocol version
		/// </summary>
		Latest = (int)Initial
	}

	/// <summary>
	/// Helper methods for compute protocol version numbers
	/// </summary>
	public static class ComputeProtocolUtilities
	{
		/// <summary>
		/// Gets the appropriate bundle options for a compute protocol version number
		/// </summary>
		public static BundleOptions GetBundleOptions(ComputeProtocol protocol)
		{
			if (protocol <= ComputeProtocol.Latest)
			{
				return new BundleOptions { MaxVersion = BundleVersion.PacketSequence };
			}
			else
			{
				throw new NotSupportedException($"Unknown compute protocol version ('{(int)protocol}')");
			}
		}
	}
}
