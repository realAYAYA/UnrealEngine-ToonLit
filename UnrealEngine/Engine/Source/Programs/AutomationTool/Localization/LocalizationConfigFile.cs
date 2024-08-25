// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System;
using UnrealBuildTool;

namespace EpicGames.Localization
{

	public enum LocalizationConfigFileFormat
	{
		LegacyMonolithic,
		Modular,

		// Add new values above this comment and upda te the value of Latest 
		Latest = Modular
	}

	public class LocalizationConfigFile
	{
		private readonly ConfigFile _configFile;
		public string Name { get; set; } = "";
		public LocalizationConfigFile()
		{
			_configFile = new ConfigFile();
		}

		public LocalizationConfigFile(string fileName)
		{
			_configFile = new ConfigFile();
			Name = fileName;
		}

		public LocalizationConfigFileSection FindOrAddSection(string sectionName)
		{
			ConfigFileSection configSection = _configFile.FindOrAddSection(sectionName);
			return new LocalizationConfigFileSection(configSection);
		}

		public void Load(string filePath)
		{
			// @TODOLocalization: Load the config file and parse all the data into the various LocalizationConfigFileParam classes 
		}

		public void Write(FileReference destinationFilePath)
		{
			// Go through all the config values and replace \ directory separators with / for uniformity across platforms
			foreach (string sectionName in _configFile.SectionNames)
			{
				ConfigFileSection section;
				if (_configFile.TryGetSection(sectionName, out section))
				{
					foreach (ConfigLine line in section.Lines)
					{
						line.Value = line.Value.Replace('\\', '/');
					}
				}
			}
			_configFile.Write(destinationFilePath);
		}
	}

	public class LocalizationConfigFileSection
	{
		private readonly ConfigFileSection _configFileSection;

		public LocalizationConfigFileSection(ConfigFileSection configFileSection)
		{
			_configFileSection = configFileSection;
		}

		public void AddValue(string key, bool value)
		{
			if (!String.IsNullOrEmpty(key))
			{
				_configFileSection.Lines.Add(new ConfigLine(ConfigLineAction.Set, key, value ? "true" : "false"));
			}
		}

		public void AddValue(string key, string value)
		{
			if (!String.IsNullOrEmpty(value) && !String.IsNullOrEmpty(key))
			{
				_configFileSection.Lines.Add(new ConfigLine(ConfigLineAction.Set, key, value));
			}
		}

		public void AddValues(string key, string[] values)
		{
			if (String.IsNullOrEmpty(key) || values.Length == 0)
			{
				return;
			}
			foreach (string value in values)
			{
				if (!String.IsNullOrEmpty(value))
				{
					_configFileSection.Lines.Add(new ConfigLine(ConfigLineAction.Set, key, value));
				}
			}
		}
	}
}
