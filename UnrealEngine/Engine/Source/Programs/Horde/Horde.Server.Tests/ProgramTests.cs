// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;
using EpicGames.Core;
using Horde.Server.Commands;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class ProgramTests
	{
		[TestMethod]
		public void TestReadGrpcCertificate()
		{
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				// Test only works on Windows as .FriendlyName cannot be set on Unix.
				return;
			}

			string friendlyName = "A testing cert";
			byte[] tempCertData = CertificateUtils.CreateSelfSignedCert("testing.epicgames.com", friendlyName);
			string tempCertPath = Path.GetTempFileName();
			File.WriteAllBytes(tempCertPath, tempCertData);

			// No cert given
			Assert.IsNull(ServerCommand.ReadGrpcCertificate(new() { ServerPrivateCert = null }));

			// Cert as file path
			{
				X509Certificate2? cert = ServerCommand.ReadGrpcCertificate(new() { ServerPrivateCert = tempCertPath });
				Assert.IsNotNull(cert);
				Assert.AreEqual(friendlyName, cert!.FriendlyName);
			}

			// Cert as base64 data
			{
				string tempCertBase64 = Convert.ToBase64String(tempCertData);
				X509Certificate2? cert = ServerCommand.ReadGrpcCertificate(new() { ServerPrivateCert = "base64:" + tempCertBase64 });
				Assert.IsNotNull(cert);
				Assert.AreEqual(friendlyName, cert!.FriendlyName);
			}

			File.Delete(tempCertPath);
		}
	}
}
