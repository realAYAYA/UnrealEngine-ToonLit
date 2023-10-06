// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Invokes the deployment handler for a target.
	/// </summary>
	[ToolMode("Deploy", ToolModeOptions.BuildPlatforms)]
	class DeployMode : ToolMode
	{
		/// <summary>
		/// If we are just running the deployment step, specifies the path to the given deployment settings
		/// </summary>
		[CommandLine("-Receipt", Required = true)]
		public FileReference? ReceiptFile = null;

		/// <summary>
		/// Execute the tool mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		/// <param name="Logger"></param>
		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			// Apply the arguments
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// Execute the deploy
			TargetReceipt Receipt = TargetReceipt.Read(ReceiptFile!);
			Logger.LogInformation("Deploying {ReceiptTargetName} {ReceiptPlatform} {ReceiptConfiguration}...", Receipt.TargetName, Receipt.Platform, Receipt.Configuration);
			UEBuildPlatform.GetBuildPlatform(Receipt.Platform).Deploy(Receipt);

			return Task.FromResult((int)CompilationResult.Succeeded);
		}
	}
}
