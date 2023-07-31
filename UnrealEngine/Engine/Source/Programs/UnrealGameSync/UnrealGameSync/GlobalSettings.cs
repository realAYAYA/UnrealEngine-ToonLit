// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace UnrealGameSync
{
	internal class GlobalPerforceSettings
	{
		public static void ReadGlobalPerforceSettings(ref string? serverAndPort, ref string? userName, ref string? depotPath, ref bool preview)
		{
			using (RegistryKey? key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Epic Games\\UnrealGameSync", false))
			{
				if (key != null)
				{
					serverAndPort = key.GetValue("ServerAndPort", serverAndPort) as string;
					userName = key.GetValue("UserName", userName) as string;
					depotPath = key.GetValue("DepotPath", depotPath) as string;
					preview = ((key.GetValue("Preview", preview? 1 : 0) as int?) ?? 0) != 0;

					// Fix corrupted depot path string
					if (depotPath != null)
					{
						Match match = Regex.Match(depotPath, "^(.*)/(Release|UnstableRelease)/\\.\\.\\.@.*$");
						if (match.Success)
						{
							depotPath = match.Groups[1].Value;
							SaveGlobalPerforceSettings(serverAndPort, userName, depotPath, preview);
						}
					}
				}
			}
		}

		public static void DeleteRegistryKey(RegistryKey rootKey, string keyName, string valueName)
		{
			using (RegistryKey? key = rootKey.OpenSubKey(keyName, true))
			{
				if (key != null)
				{
					DeleteRegistryKey(key, valueName);
				}
			}
		}

		public static void DeleteRegistryKey(RegistryKey key, string name)
		{
			string[] valueNames = key.GetValueNames();
			if (valueNames.Any(x => String.Compare(x, name, StringComparison.OrdinalIgnoreCase) == 0))
			{
				try
				{
					key.DeleteValue(name);
				}
				catch
				{
				}
			}
		}

		public static void SaveGlobalPerforceSettings(string? serverAndPort, string? userName, string? depotPath, bool preview)
		{
			try
			{
				using (RegistryKey key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\Epic Games\\UnrealGameSync"))
				{
					// Delete this legacy setting
					DeleteRegistryKey(key, "Server");

					if (String.IsNullOrEmpty(serverAndPort))
					{
						try { key.DeleteValue("ServerAndPort"); } catch (Exception) { }
					}
					else
					{
						key.SetValue("ServerAndPort", serverAndPort);
					}

					if (String.IsNullOrEmpty(userName))
					{
						try { key.DeleteValue("UserName"); } catch (Exception) { }
					}
					else
					{
						key.SetValue("UserName", userName);
					}

					if (String.IsNullOrEmpty(depotPath) || (DeploymentSettings.DefaultDepotPath != null && String.Equals(depotPath, DeploymentSettings.DefaultDepotPath, StringComparison.InvariantCultureIgnoreCase)))
					{
						DeleteRegistryKey(key, "DepotPath");
					}
					else
					{
						key.SetValue("DepotPath", depotPath);
					}

					if (preview)
					{
						key.SetValue("Preview", 1);
					}
					else
					{
						try { key.DeleteValue("Preview"); } catch (Exception) { }
					}
				}
			}
			catch (Exception ex)
			{
				MessageBox.Show("Unable to save settings.\n\n" + ex.ToString());
			}
		}
	}
}
