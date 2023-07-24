// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Windows;

namespace UnsyncUI
{
	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application
    {
		public new static App Current => Application.Current as App;

		public Config Config { get; private set; }
		public string UnsyncPath { get; private set; }
		public UserPreferences UserConfig { get; private set; }
		public string DefaultSearchTerms { get; private set; } = "";

		protected override void OnStartup(StartupEventArgs e)
		{
			string configFile = e.Args.Length > 0 ? e.Args[0] : "unsyncui.xml";

			for (int i=1; i<e.Args.Length; ++i)
			{
				bool isLast = i + 1 == e.Args.Length;
				var arg = e.Args[i];
				if (arg == "--search" && !isLast)
				{
					DefaultSearchTerms = e.Args[i + 1];
					++i;
				}
			}

			var oldWorkingDir = Environment.CurrentDirectory;

			if (File.Exists(configFile))
			{
				// Move the working dir into the same as the config file.
				// This allows relative paths to automatically be relative to the config.
				var workingDir = Path.GetDirectoryName(Path.GetFullPath(configFile));
				Environment.CurrentDirectory = workingDir;

				try
				{
					Config = new Config(configFile);
				}
				catch (Exception ex)
				{
					MessageBox.Show($"Failed to load configuration from \"{configFile}\". {ex}");
				}
			}
			else if (e.Args.Length > 0)
			{
				MessageBox.Show($"The configuration file \"{configFile}\" does not exist. Projects will not be available.");
			}

			string toolName = "unsync.exe";

			// If config specifies the explicit tool path, then use that.
			// Otherwise try same directory as config file.
			// Finally, try to look in the original working directory.

			string newWorkingDirTool = Path.GetFullPath(toolName);
			string oldWorkingDirTool = Path.GetFullPath(Path.Combine(oldWorkingDir, toolName));

			if (Config != null && Config.UnsyncPath != null)
			{
				UnsyncPath = Config.UnsyncPath;
			}
			else if (File.Exists(newWorkingDirTool))
			{
				UnsyncPath = newWorkingDirTool;
			}
			else if (File.Exists(oldWorkingDirTool))
			{
				UnsyncPath = oldWorkingDirTool;
			}

			if (UnsyncPath == null || !File.Exists(UnsyncPath))
			{
				string name = UnsyncPath ?? toolName;
				MessageBox.Show($"Failed to find \"{name}\".");
				Shutdown();
			}

			UserConfig = UserPreferences.Load();
			if (UserConfig.DFS == "")
			{
				UserConfig.DFS = null;
			}

			base.OnStartup(e);
		}

		protected override void OnExit(ExitEventArgs e)
		{
			UserConfig.Save();
			base.OnExit(e);
		}
	}

	public sealed class UserPreferences
	{
		// Change this guid if any changes are made which break compat with existing user preferences files
		private static readonly Guid version = Guid.Parse("1386020B-374C-43CE-8FBF-74AAE69EEFD3");
		private static readonly string userDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnsyncUI");
		private static readonly string userFile = Path.Combine(userDir, $"user_{version}.json");

		public string Proxy { get; set; }
		public string DFS { get; set; }
		public Dictionary<string, string> ProjectDestinationMap { get; set; } = new Dictionary<string, string>();
		public string CustomSrcPath { get; set; }
		public string CustomDstPath { get; set; }
		public string CustomInclude { get; set; }
		public string AdditionalArgs { get; set; }

		public static UserPreferences Load()
		{
			try
			{
				if (File.Exists(userFile))
				{
					return JsonSerializer.Deserialize<UserPreferences>(File.ReadAllText(userFile));
				}
			}
			catch (Exception ex)
			{
				MessageBox.Show($"Failed to load user preferences from \"{userFile}\". {ex}");
			}

			return new UserPreferences();
		}

		public void Save()
		{
			try
			{
				if (!Directory.Exists(userDir))
					Directory.CreateDirectory(userDir);

				File.WriteAllText(userFile, JsonSerializer.Serialize(this, new JsonSerializerOptions() { WriteIndented = true }));
			}
			catch { }
		}
	}
}
