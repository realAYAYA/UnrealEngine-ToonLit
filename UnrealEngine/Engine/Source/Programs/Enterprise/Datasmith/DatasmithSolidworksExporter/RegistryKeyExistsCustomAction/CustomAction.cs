// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using Microsoft.Deployment.WindowsInstaller;
using Microsoft.Win32;
using System.Diagnostics;

namespace RegistryKeyExistsCustomAction
{
	public class CustomActions
	{
		private static string CheckSolidworksInstalledSub(Session session, string swKey, string sessionKey)
		{
			string result = "";
			RegistryKey lKey202x = Registry.LocalMachine.OpenSubKey("SOFTWARE\\SolidWorks\\" + swKey + "\\Setup");
			if (lKey202x != null)
			{
				string[] names = lKey202x.GetValueNames();
				object value = null;
				foreach (var nn in names)
				{
					if (nn.ToUpper() == "SOLIDWORKS FOLDER")
					{
						value = lKey202x.GetValue("SolidWorks Folder");
						break;
					}
				}
				if (value != null)
				{
					string fullPath = Path.Combine(value as string, "solidworkstools.dll");
					if (File.Exists(fullPath))
					{
						session[sessionKey] = "1";
						result = fullPath;
					}
					else
					{
						session.Log("*ERROR* SOLIDWORKS FILES NOT FOUND");
					}
				}
				else
				{
					session.Log("*ERROR* SOLIDWORKS INSTALL FOLDER REGISTRY INFORMATION NOT FOUND");
				}
			}
			else
			{
				session.Log("*ERROR* SOLIDWORKS INSTALL FOLDER REGISTRY KEY INFO NOT FOUND");
			}

			return result;
		}

		[CustomAction]
		public static ActionResult CheckSolidworksInstalled(Session session)
		{
			var tools2020 = CheckSolidworksInstalledSub(session, "SOLIDWORKS 2020", "SOLIDWORKS2020INSTALLED");
			var tools2021 = CheckSolidworksInstalledSub(session, "SOLIDWORKS 2021", "SOLIDWORKS2021INSTALLED");
			var tools2022 = CheckSolidworksInstalledSub(session, "SOLIDWORKS 2022", "SOLIDWORKS2022INSTALLED");
			var tools2023 = CheckSolidworksInstalledSub(session, "SOLIDWORKS 2023", "SOLIDWORKS2023INSTALLED");
			var tools2024 = CheckSolidworksInstalledSub(session, "SOLIDWORKS 2024", "SOLIDWORKS2024INSTALLED");
			// SOLIDWORKSTOOLSPATH is unused(not installing solidworkstools.dll anymore)
			// if (!string.IsNullOrEmpty(tools2022))
			// 	session["SOLIDWORKSTOOLSPATH"] = tools2022;
			// else if (!string.IsNullOrEmpty(tools2021))
			// 	session["SOLIDWORKSTOOLSPATH"] = tools2021;
			// else if (!string.IsNullOrEmpty(tools2020))
			// 	session["SOLIDWORKSTOOLSPATH"] = tools2020;

			return ActionResult.Success;
		}
	}
}
