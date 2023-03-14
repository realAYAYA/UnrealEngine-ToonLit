// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Auth;
using EpicGames.Horde.Storage.Impl;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using System;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Settings to configure a connected Horde.Storage instance
	/// </summary>
	public class StorageOptions : HttpServiceClientOptions
	{
	}

	/// <summary>
	/// Extension methods for configuring Horde.Storage
	/// </summary>
	public static class StorageExtensions
	{
		/// <summary>
		/// Registers services for Horde Storage
		/// </summary>
		/// <param name="services">The current service collection</param>
		public static void AddHordeStorage(this IServiceCollection services)
		{
			services.AddOptions<StorageOptions>();

			services.AddScoped<IStorageClient, HttpStorageClient>();
			services.AddHttpClientWithAuth<IStorageClient, HttpStorageClient>(serviceProvider => serviceProvider.GetRequiredService<IOptions<StorageOptions>>().Value);
		}

		/// <summary>
		/// Registers services for Horde Storage
		/// </summary>
		/// <param name="services">The current service collection</param>
		/// <param name="configure">Callback for configuring the storage service</param>
		public static void AddHordeStorage(this IServiceCollection services, Action<StorageOptions> configure)
		{
			AddHordeStorage(services);
			services.Configure<StorageOptions>(configure);
		}
	}
}
