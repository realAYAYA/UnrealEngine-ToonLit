// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Generates documentation from reflection data
	/// </summary>
	[ToolMode("WriteDocumentation", ToolModeOptions.None)]
	class WriteDocumentationMode : ToolMode
	{
		/// <summary>
		/// Enum for the type of documentation to generate
		/// </summary>
		enum DocumentationType
		{
			BuildConfiguration,
			ModuleRules,
			TargetRules,
		}

		/// <summary>
		/// Type of documentation to generate
		/// </summary>
		[CommandLine(Required = true)]
		DocumentationType Type = DocumentationType.BuildConfiguration;

		/// <summary>
		/// The HTML file to write to
		/// </summary>
		[CommandLine(Required = true)]
		FileReference OutputFile = null!;

		/// <summary>
		/// Entry point for this command
		/// </summary>
		/// <returns></returns>
		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			switch (Type)
			{
				case DocumentationType.BuildConfiguration:
					XmlConfig.WriteDocumentation(OutputFile, Logger);
					break;
				case DocumentationType.ModuleRules:
					RulesDocumentation.WriteDocumentation(typeof(ModuleRules), OutputFile, Logger);
					break;
				case DocumentationType.TargetRules:
					RulesDocumentation.WriteDocumentation(typeof(TargetRules), OutputFile, Logger);
					break;
				default:
					throw new BuildException("Invalid documentation type: {0}", Type);
			}
			return Task.FromResult(0);
		}
	}
}
