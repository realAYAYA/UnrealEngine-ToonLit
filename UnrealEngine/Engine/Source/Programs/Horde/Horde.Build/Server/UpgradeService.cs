// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Server;
using Horde.Build.Users;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Server
{
	/// <summary>
	/// Service to perform schema upgrades on the database
	/// </summary>
	public class UpgradeService
	{
		/// <summary>
		/// The current schema version
		/// </summary>
		public const int LatestSchemaVersion = 5;

		/// <summary>
		/// The database service instance
		/// </summary>
		readonly MongoService _mongoService;

		/// <summary>
		/// The DI service provider
		/// </summary>
		readonly IServiceProvider _serviceProvider;

		/// <summary>
		/// Logger instance
		/// </summary>
		readonly ILogger<UpgradeService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		/// <param name="serviceProvider">The DI service provider</param>
		/// <param name="logger">Logger instance</param>
		public UpgradeService(MongoService mongoService, IServiceProvider serviceProvider, ILogger<UpgradeService> logger)
		{
			_mongoService = mongoService;
			_serviceProvider = serviceProvider;
			_logger = logger;
		}

		/// <summary>
		/// Update the database schema
		/// </summary>
		/// <param name="fromVersion">The version to upgrade from. Automatically detected if this is null.</param>
		/// <returns>Async task</returns>
		public async Task UpgradeSchemaAsync(int? fromVersion)
		{
			Globals globals = await _mongoService.GetGlobalsAsync();
			int schemaVersion = fromVersion ?? globals.SchemaVersion ?? 0;

			while (schemaVersion < LatestSchemaVersion)
			{
				_logger.LogInformation("Upgrading from schema version {Version}", schemaVersion);

				// Handle the current version
				//				if (SchemaVersion == 0)
				//				{
				//					IssueCollection IssueCollection = new IssueCollection(DatabaseService);
				//					IJobCollection JobCollection = ServiceProvider.GetService<IJobCollection>();
				//					await IssueCollection.UpdateSchemaLogIdFields(JobCollection, Logger);
				//				}
				//				if(SchemaVersion == 2)
				//				{
				//					IssueCollection IssueCollection = new IssueCollection(DatabaseService);
				//					await IssueCollection.FixResolvedIssuesAsync(Logger);
				//				}

				if (schemaVersion == 3)
				{
					IJobCollection jobCollection = _serviceProvider.GetRequiredService<IJobCollection>();
					await jobCollection.UpgradeDocumentsAsync();
				}
				if (schemaVersion == 4)
				{
					UserCollectionV1 userCollectionV1 = new UserCollectionV1(_serviceProvider.GetRequiredService<MongoService>());
					using UserCollectionV2 userCollectionV2 = new UserCollectionV2(_serviceProvider.GetRequiredService<MongoService>(), _serviceProvider.GetRequiredService<ILogger<UserCollectionV2>>());
					await userCollectionV2.ResaveDocumentsAsync(userCollectionV1);
				}

				// Increment the version number
				schemaVersion++;

				// Try to update the current schema version number
				while (globals.SchemaVersion == null || globals.SchemaVersion < schemaVersion)
				{
					globals.SchemaVersion = schemaVersion;
					if (await _mongoService.TryUpdateSingletonAsync(globals))
					{
						break;
					}
					globals = await _mongoService.GetGlobalsAsync();
				}
			}
		}
	}
}
