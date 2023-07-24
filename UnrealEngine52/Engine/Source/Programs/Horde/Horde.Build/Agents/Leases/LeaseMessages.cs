// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Build.Agents.Leases
{

	/// <summary>
	/// Updates an existing lease
	/// </summary>
	public class UpdateLeaseRequest
	{
		/// <summary>
		/// Mark this lease as aborted
		/// </summary>
		public bool? Aborted { get; set; }
	}
}
