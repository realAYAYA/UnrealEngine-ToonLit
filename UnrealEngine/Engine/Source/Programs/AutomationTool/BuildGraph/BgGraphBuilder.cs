// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using EpicGames.BuildGraph.Expressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
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
		/// Accessor for default logger instance
		/// </summary>
		protected static ILogger Logger => Log.Logger;

		/// <summary>
		/// Callback used to instantiate the graph
		/// </summary>
		/// <param name="env">The graph context</param>
		public abstract BgGraph CreateGraph(BgEnvironment env);
	}
}
