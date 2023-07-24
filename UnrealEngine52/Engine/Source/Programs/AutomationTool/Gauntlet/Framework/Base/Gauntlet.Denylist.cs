// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Newtonsoft.Json;
using UnrealBuildTool;

namespace Gauntlet
{

	/// <summary>
	/// Information that defines a device
	/// </summary>
	public class DenylistEntry
	{
		public string TestName;

		public string[] Platforms;

		public string BranchName;
		public DenylistEntry()
		{
			TestName = "None";
			Platforms = new string [] { };
			BranchName = "None";
		}

		public override string ToString()
		{
			return string.Format("{0} Platforms={1} Branch={2}", TestName, string.Join(",",Platforms.ToList()), BranchName);
		}
	}


	public class Denylist
	{
		/// <summary>
		/// Static instance
		/// </summary>
		private static Denylist _Instance;

		IEnumerable<DenylistEntry> DenylistEntries;

		/// <summary>
		/// Protected constructor - code should use DevicePool.Instance
		/// </summary>
		protected Denylist()
		{
			if (_Instance == null)
			{
				_Instance = this;
			}

			DenylistEntries = new DenylistEntry[0] { };

			// temp, pass in
			LoadDenylist(@"P:\Builds\Automation\Fortnite\Config\denylist.json");
		}

		/// <summary>
		/// Access to our singleton
		/// </summary>
		public static Denylist Instance
		{
			get
			{
				if (_Instance == null)
				{
					new Denylist();
				}
				return _Instance;
			}
		}

		protected void LoadDenylist(string InFilePath)
		{
			if (File.Exists(InFilePath))
			{
				try
				{ 
					Gauntlet.Log.Info("Loading denylist from {0}", InFilePath);
					List<DenylistEntry> NewEntries = JsonConvert.DeserializeObject<List<DenylistEntry>>(File.ReadAllText(InFilePath));

					if (NewEntries != null)
					{
						DenylistEntries = NewEntries;
					}

					// cannonical branch format is ++
					DenylistEntries = DenylistEntries.Select(E =>
					{
						E.BranchName = E.BranchName.Replace("/", "+");
						return E;
					});
				}
				catch (Exception Ex)
				{
				Log.Warning("Failed to load denylist file {0}. {1}", InFilePath, Ex.Message);
				}
				
			}
		}
		public bool IsTestDenylisted(string InNodeName, UnrealTargetPlatform InPlatform, string InBranchName)
		{
			// find any references to this test irrespective of platform & branch
			IEnumerable<DenylistEntry> Entries = DenylistEntries.Where(E => E.TestName == InNodeName);

			string NormalizedBranchName = InBranchName.Replace("/", "+");

			// Filter by branch
			Entries = Entries.Where(E => E.BranchName == "*" || string.Equals(E.BranchName, NormalizedBranchName, StringComparison.OrdinalIgnoreCase));

			// Filter by branch
			Entries = Entries.Where(E => E.Platforms.Length == 0 || E.Platforms.Contains(InPlatform.ToString()));

			return Entries.Count() > 0;
		}
	}
}
