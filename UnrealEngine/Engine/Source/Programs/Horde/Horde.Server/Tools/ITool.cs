// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Describes a standalone, external tool hosted and deployed by Horde. Provides basic functionality for performing
	/// gradual roll-out, versioning, etc...
	/// </summary>
	public interface ITool
	{
		/// <summary>
		/// Identifier for the tool
		/// </summary>
		ToolId Id { get; }

		/// <summary>
		/// Current deployments of this tool, sorted by time.
		/// </summary>
		IReadOnlyList<IToolDeployment> Deployments { get; }

		/// <summary>
		/// Config object for this tool
		/// </summary>
		ToolConfig Config { get; }
	}

	/// <summary>
	/// Deployment of a tool
	/// </summary>
	public interface IToolDeployment
	{
		/// <summary>
		/// Identifier for this deployment. A new identifier will be assigned to each created instance, so an identifier corresponds to a unique deployment.
		/// </summary>
		ToolDeploymentId Id { get; }

		/// <summary>
		/// Descriptive version string for this tool revision
		/// </summary>
		string Version { get; }

		/// <summary>
		/// Current state of this deployment
		/// </summary>
		ToolDeploymentState State { get; }

		/// <summary>
		/// Current progress of the deployment
		/// </summary>
		double Progress { get; }

		/// <summary>
		/// Last time at which the progress started. Set to null if the deployment was paused.
		/// </summary>
		DateTime? StartedAt { get; }

		/// <summary>
		/// Length of time over which to make the deployment
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// Namespace containing the tool
		/// </summary>
		NamespaceId NamespaceId { get; }

		/// <summary>
		/// Reference to this tool in Horde Storage.
		/// </summary>
		RefName RefName { get; }
	}

	/// <summary>
	/// Extension methods for tools
	/// </summary>
	public static class ToolExtensions
	{
		/// <summary>
		/// Gets the current deployment
		/// </summary>
		/// <param name="tool">Tool to query</param>
		/// <param name="phase">Adoption phase for the caller. 0 which the </param>
		/// <param name="utcNow">Current time</param>
		/// <returns></returns>
		public static IToolDeployment? GetCurrentDeployment(this ITool tool, double phase, DateTime utcNow)
		{
			int idx = tool.Deployments.Count - 1;
			for (; idx >= 0; idx--)
			{
				if (phase <= tool.Deployments[idx].GetProgressValue(utcNow) || idx == 0)
				{
					return tool.Deployments[idx];
				}
			}
			return null;
		}

		/// <summary>
		/// Get the progress fraction for a particular deployment and time
		/// </summary>
		/// <param name="deployment"></param>
		/// <param name="utcNow"></param>
		/// <returns></returns>
		public static double GetProgressValue(this IToolDeployment deployment, DateTime utcNow)
		{
			if (deployment.StartedAt == null)
			{
				return deployment.Progress;
			}
			else if (deployment.Duration > TimeSpan.Zero)
			{
				return Math.Clamp((utcNow - deployment.StartedAt.Value) / deployment.Duration, 0.0, 1.0);
			}
			else
			{
				return 1.0;
			}
		}
	}
}
