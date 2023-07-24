// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Serialization;
using System;

#pragma warning disable CS1591
#pragma warning disable CA1819

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Terminate the current connection
	/// </summary>
	[ComputeMessage("close"), CbObject]
	public class CloseMessage
	{
	}

	/// <summary>
	/// XOR a block of data with a value
	/// </summary>
	[ComputeMessage("xor-req")]
	public class XorRequestMessage
	{
		[CbField]
		public byte Value { get; set; }

		[CbField]
		public byte[] Payload { get; set; } = Array.Empty<byte>();
	}

	/// <summary>
	/// Response from an XOR message
	/// </summary>
	[ComputeMessage("xor-rsp")]
	public class XorResponseMessage
	{
		[CbField]
		public byte[] Payload { get; set; } = Array.Empty<byte>();
	}
}
