// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;
using System.Xml.Linq;
using Microsoft.Deployment.WindowsInstaller;
using Microsoft.Win32;

namespace CustomActionManaged
{
	/// <summary>
	/// Helper that exposes misc environment info.
	/// </summary>
	public static class CustomEnvironmentInfo
	{
		[DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
		[return: MarshalAs(UnmanagedType.Bool)]
		private static extern bool IsWow64Process(
			[In] IntPtr hProcess,
			[Out] out bool wow64Process
		);

		private static bool InternalIs64BitOperatingSystem()
		{
			bool RetVal = false;

			try
			{
				using (Process p = Process.GetCurrentProcess())
				{
					if (!IsWow64Process(p.Handle, out RetVal))
					{
						return false;
					}
				}
			}
			catch (System.Exception)
			{

			}
			return RetVal;
		}

		// Normally we would use Environment.Is64BitOperatingSystem but this is a .NET 4.0 property and we want to stick to 2.0 to cover
		//  all potential release platforms.
		public static bool Is64BitOperatingSystem = (IntPtr.Size == 8) || InternalIs64BitOperatingSystem();
	}

	public class CustomActions
	{
		[DllImport("Advapi32.dll")]
		static extern uint RegOpenKeyEx(
			UIntPtr hKey,
			string lpSubKey,
			uint ulOptions,
			int samDesired,
			out UIntPtr phkResult);

		[DllImport("Advapi32.dll")]
		static extern uint RegCloseKey(UIntPtr hKey);

		[DllImport("advapi32.dll", EntryPoint = "RegQueryValueEx")]
		public static extern int RegQueryValueEx(
			UIntPtr hKey, string lpValueName,
			int lpReserved,
			ref uint lpType,
			System.Text.StringBuilder lpData,
			ref uint lpcbData);

		public static UIntPtr HKEY_LOCAL_MACHINE = new UIntPtr(0x80000002u);
		public static UIntPtr HKEY_CURRENT_USER = new UIntPtr(0x80000001u);

		public static int QueryValue = 0x0001;
		public static int WOW64_64Key = 0x0100;

		/// <summary>
		/// Helper function for to get session properties in a safe way.  Returns the empty string if the property is not found.
		/// </summary>
		private static string GetSessionProperty(Session session, string PropertyName)
		{
			string PropertyValue = string.Empty;
			try
			{
				PropertyValue = session[PropertyName];
			}
			catch (System.Exception ex)
			{
				PropertyValue = string.Empty;
				session.Log("Failed to get session property {0}: {1}", PropertyName, ex.Message);
			}
			return PropertyValue;
		}

		/// <summary>
		/// Helper function for setting session properties.
		/// </summary>
		private static void SetSessionProperty(Session session, string PropertyName, string Value)
		{
			try
			{
				session[PropertyName] = Value;
			}
			catch (System.Exception ex)
			{
				session.Log("Failed to set session property {0}: {1}", PropertyName, ex.Message);
			}
		}

		[CustomAction]
		public static ActionResult SearchInstallationPaths(Session session)
		{
			try
			{
				string[] MajorVersions = new string[] { "27", "26", "25", "24", "23", };

				string[] MinorVersions = new string[] { "0", };

				string[] Products = new string[] { "FULL", "SOLO", };

				string[] Releases = new string[] { "R1-1", };

				string[] Languages = new string[] {
					"INT", "GER", "USA", "UKI", "HUN", "FRA", "CHE", "AUT", "CHI", "CZE", "FIN", "GRE",
					"NED", "ITA", "JPN", "KOR", "POL", "POR", "RUS", "SPA", "SWE", "TAI", "NOR", "NZE",
					"AUS", "TUR", "BRA", "UKR",
				};

				string RegistryKeyFormat = "SOFTWARE\\GRAPHISOFT Installers\\ARCHICAD\\ARCHICAD {0}.{1} {2} {3} {4}";

				foreach(string MajorVersion in MajorVersions)
				{
					foreach (string Product in Products)
					{
						// Check if ArchiCAD for this major version and this product is installed on the computer
						bool bKeyEntryFound = false;

						foreach (string MinorVersion in MinorVersions)
						{
							foreach (string Language in Languages)
							{
								foreach (string Release in Releases)
								{
									// Build key path from version, language and product
									string RegistryKeyPath = string.Format(RegistryKeyFormat, MajorVersion, MinorVersion, Language, Product, Release);

									UIntPtr Key = UIntPtr.Zero;
									StringBuilder DataSB = new StringBuilder(1024);

									try
									{
										if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegistryKeyPath, 0, QueryValue | WOW64_64Key, out Key) == 0)
										{
											uint Type = 0;
											uint DataSize = 1024;
											int Ret = RegQueryValueEx(Key, "InstallLocation", 0, ref Type, DataSB, ref DataSize);

											bKeyEntryFound = DataSB.Length > 0;

											bKeyEntryFound = true;
										}
									}
									finally
									{
										if (Key != UIntPtr.Zero)
										{
											RegCloseKey(Key);
										}
									}

									if (bKeyEntryFound)
									{
										string InstallLocationDir = DataSB.ToString();

										string[] ColladaInOutFiles = Directory.GetFiles(InstallLocationDir, "Collada In-Out.apx", SearchOption.AllDirectories);
										if(ColladaInOutFiles.Count() > 0)
										{
											string AddOnInOutDir = Path.GetDirectoryName(ColladaInOutFiles[0]);

											// Update property storing install directory
											string PropertyToUpdate = string.Format("ARCHICAD{0}{1}DIR", MajorVersion, Product);
											SetSessionProperty(session, PropertyToUpdate, InstallLocationDir);

											PropertyToUpdate = string.Format("ARCHICAD{0}{1}ADDONSDIR", MajorVersion, Product);
											SetSessionProperty(session, PropertyToUpdate, AddOnInOutDir);
										}

										break;
									}
								}

								// Property has been no need to look ay further
								if (bKeyEntryFound == true)
								{
									break;
								}
							}

							// Property has been no need to look ay further
							if (bKeyEntryFound == true)
							{
								break;
							}
						}
					}
				}

				session.Log("Successfully update option's values.");
			}
			catch (System.Exception)
			{
				session.Log("Failed to update option's values.");
			}

			// We always return success even if the procedure to set engine start tab failed.  This will prevent
			//  the installer from failing and rolling back.
			return ActionResult.Success;
		}

	}
}

