// Copyright Epic Games, Inc. All Rights Reserved.

#if DISABLE
using System.Reflection;
using EpicGames.Core;
using EpicGames.UHT.Utils;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;
using UnrealBuildTool.Modes;
#endif
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnrealBuildToolTests
{
	[TestClass]
	public class UnrealHeaderToolTests
	{
		[TestMethod]
		public void Run()
		{
#if DISABLE
			FileReference AssemblyFile = new FileReference(Assembly.GetExecutingAssembly().Location);
			Unreal.LocationOverride.RootDirectory = DirectoryReference.Combine(AssemblyFile.Directory, "../../../../..");

			string[] Arguments = System.Array.Empty<string>();
			CommandLineArguments CommandLineArguments = new CommandLineArguments(Arguments);
			UhtGlobalOptions Options = new UhtGlobalOptions(CommandLineArguments);

			// Initialize the attributes
			UhtTables Tables = new UhtTables();

			// Initialize the configuration
			IUhtConfig Config = new UhtConfigImpl(CommandLineArguments);

			// Run the tests
			using ILoggerFactory factory = LoggerFactory.Create(x => x.AddEpicDefault());
			Assert.IsTrue(UhtTestHarness.RunTests(Tables, Config, Options, factory.CreateLogger<UhtTestHarness>()));
#endif
		}
	}
}
