// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// Extension methods for Horde
	/// </summary>
	public static class HordeExtensions
	{
		/// <summary>
		/// Adds Horde-related services with the default settings
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		public static void AddHorde(this IServiceCollection serviceCollection)
		{
			serviceCollection.AddHordeHttpClient();
			serviceCollection.AddSingleton<BundleCache>(sp => new BundleCache(sp.GetRequiredService<IOptions<HordeOptions>>().Value.BundleCache));
			serviceCollection.AddSingleton<StorageBackendCache>();
			serviceCollection.AddSingleton<HttpStorageBackendFactory>();
			serviceCollection.AddSingleton<HttpStorageClientFactory>();
			serviceCollection.AddSingleton<IHordeClient, HordeClient>();
		}

		/// <summary>
		/// Adds Horde-related services
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		/// <param name="configureHorde">Callback to configure options</param>
		public static void AddHorde(this IServiceCollection serviceCollection, Action<HordeOptions> configureHorde)
		{
			serviceCollection.Configure(configureHorde);
			AddHorde(serviceCollection);
		}
	}
}
