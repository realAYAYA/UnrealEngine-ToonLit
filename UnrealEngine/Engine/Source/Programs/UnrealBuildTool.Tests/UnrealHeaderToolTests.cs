// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildBase;
using EpicGames.Core;
using EpicGames.UHT.Utils;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool.Modes;
using System.Reflection;

namespace UnrealBuildToolTests
{
	[TestClass]
	public class UnrealHeaderToolTests
	{
		[TestMethod]
		public void Run()
		{
			FileReference AssemblyFile = new FileReference(Assembly.GetExecutingAssembly().Location);
			Unreal.LocationOverride.RootDirectory = DirectoryReference.Combine(AssemblyFile.Directory, "../../../../..");

			string[] Arguments = new string[] { };
			CommandLineArguments CommandLineArguments = new CommandLineArguments(Arguments);
			UhtGlobalOptions Options = new UhtGlobalOptions(CommandLineArguments);

			// Initialize the attributes
			UhtTables Tables = new UhtTables();

			// Initialize the configuration
			IUhtConfig Config = new UhtConfigImpl(CommandLineArguments);

			// Run the tests
			ILoggerFactory factory = LoggerFactory.Create(x => x.AddEpicDefault());
			Assert.IsTrue(UhtTestHarness.RunTests(Tables, Config, Options, factory.CreateLogger<UhtTestHarness>()));
		}
	}
}
