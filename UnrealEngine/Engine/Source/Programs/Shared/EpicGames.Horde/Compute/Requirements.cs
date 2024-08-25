// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Common;
using EpicGames.Serialization;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Stores information about a directory in an action's workspace
	/// </summary>
	public class Requirements
	{
		/// <summary>
		/// Pool of machines to draw from
		/// </summary>
		[CbField("p")]
		public string? Pool { get; set; }

		/// <summary>
		/// Condition string to be evaluated against the machine spec, eg. cpu-cores >= 10 &amp;&amp; ram.mb >= 200 &amp;&amp; pool == 'worker'
		/// </summary>
		[CbField("c")]
		public Condition? Condition { get; set; }

		/// <summary>
		/// Resources used by the process
		/// </summary>
		[CbField("r")]
		public Dictionary<string, ResourceRequirements> Resources { get; } = new Dictionary<string, ResourceRequirements>();

		/// <summary>
		/// Whether we require exclusive access to the device
		/// </summary>
		[CbField("e")]
		public bool Exclusive { get; set; }

		/// <summary>
		/// Default constructor
		/// </summary>
		public Requirements()
		{
		}

		/// <summary>
		/// Construct a requirements object with a condition
		/// </summary>
		/// <param name="condition">Condition for matching machines to execute the work</param>
		public Requirements(Condition? condition)
		{
			Condition = condition;
		}

		/// <summary>
		/// Serialize this object to bytes
		/// </summary>
		public byte[] Serialize()
		{
			CbWriter writer = new CbWriter();
			CbSerializer.Serialize(writer, this);
			return writer.ToByteArray();
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			List<string> list = new List<string>();
			if (Pool != null)
			{
				list.Add($"Pool:{Pool}");
			}
			if (Condition != null)
			{
				list.Add($"\"{Condition}\"");
			}
			foreach ((string name, ResourceRequirements allocation) in Resources)
			{
				list.Add($"{name}: {allocation.Min}-{allocation.Max}");
			}
			if (Exclusive)
			{
				list.Add("Exclusive");
			}
			return String.Join(", ", list);
		}
	}

	/// <summary>
	/// Specifies requirements for resource allocation
	/// </summary>
	public class ResourceRequirements
	{
		/// <summary>
		/// Minimum allocation of the requested resource
		/// </summary>
		[CbField("min")]
		public int Min { get; set; } = 1;

		/// <summary>
		/// Maximum allocation of the requested resource. Allocates as much as possible unless capped.
		/// </summary>
		[CbField("max")]
		public int? Max { get; set; }
	}
}
