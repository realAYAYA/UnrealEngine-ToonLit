// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using Horde.Server.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Configuration;

[TestClass]
public class ConfigServiceTests
{
	[TestMethod]
	public void GetGlobalConfigUriTest()
	{
		// Paths are handled differently depending on OS deep inside Path.* methods in .NET SDK
		// So do different tests depending on platform

		if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
		{
			Console.WriteLine("Ran Windows");
			AssertConfig("globals.json", "C:\\SomeHordeDir", "file:///C:/SomeHordeDir/globals.json");
			AssertConfig("C:\\OtherDir\\globals.json", "C:\\SomeHordeDir", "file:///C:/OtherDir/globals.json");
		}

		if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
		{
			Console.WriteLine("Ran Linux");
			AssertConfig("globals.json", "/app", "file:///app/globals.json");
			AssertConfig("/some/dir/globals.json", "/app", "file:///some/dir/globals.json");
		}

		AssertConfig("//depot/dir1/globals.json", "C:\\SomeHordeDir", "perforce://default//depot/dir1/globals.json");
	}

	private static void AssertConfig(string configPath, string configDir, string expectedPath)
	{
		Uri uri = ConfigService.GetGlobalConfigUri(configPath, configDir);
		Assert.AreEqual(expectedPath, uri.AbsoluteUri);
		Assert.IsTrue(uri.IsAbsoluteUri);
	}
}

