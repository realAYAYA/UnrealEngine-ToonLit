// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Jupiter;

namespace BuildScripts.Automation
{
	[Help("Downloads a build from Jupiter ")]
	class DownloadJupiterBuild : BuildCommand
	{
		
		public override void ExecuteBuild()
		{
			string Key = ParseRequiredStringParam("key");
			string Namespace = ParseRequiredStringParam("namespace");
			string JupiterUrl = ParseRequiredStringParam("url");
			DirectoryReference TargetDirectory = ParseRequiredDirectoryReferenceParam("targetdirectory");

			JupiterFileTree Tree = new JupiterFileTree(TargetDirectory, true);
			
			FileReference LocalManifest = FileReference.Combine(TargetDirectory, "Jupiter-Manifest.json");
			Task DownloadTask = Tree.DownloadFromJupiter(LocalManifest, JupiterUrl, Namespace, Key);
			DownloadTask.Wait();
		}
	}
}
