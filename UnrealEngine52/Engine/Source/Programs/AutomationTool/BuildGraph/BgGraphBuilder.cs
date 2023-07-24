// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using EpicGames.BuildGraph.Expressions;
using System;
using System.Collections.Generic;
using System.Text;

namespace AutomationTool
{
	/// <summary>
	/// Base class for any user defined graphs
	/// </summary>
	public abstract class BgGraphBuilder
	{
		/// <summary>
		/// Callback used to instantiate the graph
		/// </summary>
		/// <param name="env">The graph context</param>
		public abstract BgGraph CreateGraph(BgEnvironment env);
	}
}
