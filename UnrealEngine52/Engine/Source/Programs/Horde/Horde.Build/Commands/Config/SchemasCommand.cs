// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Commands.Config
{
	[Command("config", "schemas", "Writes JSON schemas for server settings")]
	class SchemasCommand : Command
	{
		[CommandLine]
		DirectoryReference? _outputDir = null!;

		public override Task<int> ExecuteAsync(ILogger logger)
		{
			_outputDir ??= DirectoryReference.Combine(Program.AppDir, "Schemas");

			DirectoryReference.CreateDirectory(_outputDir);
			foreach (Type schemaType in SchemaController.ConfigSchemas)
			{
				FileReference outputFile = FileReference.Combine(_outputDir, $"{schemaType.Name}.json");
				Schemas.CreateSchema(schemaType).Write(outputFile);
			}

			return Task.FromResult(0);
		}
	}
}
