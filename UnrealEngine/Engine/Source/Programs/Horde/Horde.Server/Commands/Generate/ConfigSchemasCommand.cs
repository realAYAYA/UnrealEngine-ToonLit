// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Generate
{
	[Command("generate", "configschemas", "Writes JSON schemas for server settings")]
	class SchemasCommand : Command
	{
		[CommandLine("-OutputDir=")]
		[Description("Output directory to write schemas to. Defaults to the 'Schemas' subfolder of the application directory.")]
		DirectoryReference? _outputDir = null!;

		public override Task<int> ExecuteAsync(ILogger logger)
		{
			_outputDir ??= DirectoryReference.Combine(ServerApp.AppDir, "Schemas");

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
