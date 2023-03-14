// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Common;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Stores information about a directory in an action's workspace
	/// </summary>
	public class Requirements
	{
		/// <summary>
		/// Condition string to be evaluated against the machine spec, eg. cpu-cores >= 10 &amp;&amp; ram.mb >= 200 &amp;&amp; pool == 'worker'
		/// </summary>
		[CbField("c")]
		public Condition? Condition { get; set; }

		/// <summary>
		/// Resources used by the process
		/// </summary>
		[CbField("r")]
		public Dictionary<string, int> Resources { get; } = new Dictionary<string, int>();

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
		/// <param name="condition"></param>
		public Requirements(Condition? condition)
		{
			Condition = condition;
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			List<string> list = new List<string>();
			if (Condition != null)
			{
				list.Add($"\"{Condition}\"");
			}
			foreach ((string name, int count) in Resources)
			{
				list.Add($"{name}: {count}");
			}
			if (Exclusive)
			{
				list.Add("Exclusive");
			}
			return String.Join(", ", list);
		}
	}
}
