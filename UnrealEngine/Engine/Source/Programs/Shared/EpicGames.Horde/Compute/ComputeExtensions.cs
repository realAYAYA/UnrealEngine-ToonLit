// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Auth;
using EpicGames.Horde.Compute.Impl;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using System;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Settings to configure a connected Horde.Build instance
	/// </summary>
	public class ComputeOptions : HttpServiceClientOptions
	{
	}

	/// <summary>
	/// Extension methods for configuring Horde Compute
	/// </summary>
	public static class ComputeExtensions
	{
		/// <summary>
		/// Registers services for Horde Compute
		/// </summary>
		/// <param name="services">The current service collection</param>
		public static void AddHordeCompute(this IServiceCollection services)
		{
			services.AddOptions<ComputeOptions>();

			services.AddScoped<IComputeClient, HttpComputeClient>();
			services.AddHttpClientWithAuth<IComputeClient, HttpComputeClient>(serviceProvider => serviceProvider.GetRequiredService<IOptions<ComputeOptions>>().Value);
		}

		/// <summary>
		/// Registers services for Horde Compute
		/// </summary>
		/// <param name="services">The current service collection</param>
		/// <param name="configure">Callback for configuring the storage service</param>
		public static void AddHordeCompute(this IServiceCollection services, Action<ComputeOptions> configure)
		{
			AddHordeCompute(services);
			services.Configure<ComputeOptions>(configure);
		}
	}
}
