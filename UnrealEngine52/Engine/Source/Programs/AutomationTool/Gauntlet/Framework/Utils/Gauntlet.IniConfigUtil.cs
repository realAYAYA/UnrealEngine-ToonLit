// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;

namespace Gauntlet
{

	public static class IniConfigUtil
	{
		/// <summary>
		/// Get a ConfigHierarchy of the type you want with platform-specific config for the given Role. This object can be used to read .ini config values.
		/// Default params return the client platform's game config.
		/// This looks for the workspace files for the project that you are trying to run your test on. The config directory for that project must exist to find valid results.
		/// </summary>
		public static ConfigHierarchy GetConfigHierarchy(UnrealTestContext TestContext, ConfigHierarchyType ConfigType = ConfigHierarchyType.Game, UnrealTargetRole TargetRole = UnrealTargetRole.Client)
		{
			string ProjectPath = Path.Combine(Environment.CurrentDirectory, TestContext.BuildInfo.ProjectName);
			if (!Directory.Exists(ProjectPath))
			{
				Log.Warning(string.Format("Directory does not exist at {0}! Returned ConfigHierarchy will not contain any config values. Make sure to sync the config directory for the project you are trying to run.", ProjectPath));
			}

			return ConfigCache.ReadHierarchy(ConfigType, new DirectoryReference(ProjectPath), TestContext.GetRoleContext(TargetRole).Platform);
		}

		/// <summary>
		/// Helper function to return a list of separated values from a found config value string.
		/// </summary>
		/// <example>
		/// Given a config section:
		/// <code>
		/// [/Script/FortniteGame.FortProfileGo]
		/// ProfileGoScenarios=(Name="BestCase", Position=(X=108298.719,Y=-131650.031,Z=-3136.548))
		/// </code>
		/// The following code will set the value of MyScenarios to a List of one string with a value of '(Name="BestCase", Position=(X=108298.719,Y=-131650.031,Z=-3136.548))'
		/// <code>
		/// GetConfigHierarchy(Context).TryGetValues("/Script/FortniteGame.FortProfileGo", "ProfileGoScenarios", out MyScenarios);
		/// </code>
		/// Using this functions as follows will return a List of two strings whose values are 'Name="BestCase"' and 'Position=(X=108298.719,Y=-131650.031,Z=-3136.548)'
		/// <code>
		/// ParseListFromConfigString(MyScenarios);
		/// </code>
		/// </example>
		public static List<string> ParseListFromConfigString(string ConfigString, char ElementSeparator = ',')
		{
			string ObjectConfigString = ConfigString;
			if ((ObjectConfigString.StartsWith("(") && ObjectConfigString.EndsWith(")")) || (ObjectConfigString.StartsWith("\"") && ObjectConfigString.EndsWith("\"")))
			{
				ObjectConfigString = ObjectConfigString.Substring(1).Substring(0, ObjectConfigString.Length - 2);
			}

			List<string> FoundElements = new List<string>();
			string TempElement = string.Empty;
			Stack<char> SubstringEndChars = new Stack<char>();
			for (int StringIndex = 0; StringIndex < ObjectConfigString.Length; StringIndex++)
			{
				char CurrentChar = ObjectConfigString.ElementAt(StringIndex);
				if (SubstringEndChars.Count > 0 && CurrentChar == SubstringEndChars.Peek())
				{
					SubstringEndChars.Pop();
				}
				else if (CurrentChar == '"')
				{
					// Doesn't handle nested quotes, probably shouldn't
					SubstringEndChars.Push('"');
				}
				else if (CurrentChar == '(')
				{
					SubstringEndChars.Push(')');
				}
				else if (SubstringEndChars.Count == 0 && CurrentChar == ElementSeparator)
				{
					FoundElements.Add(TempElement.Trim());
					TempElement = string.Empty;
					continue;
				}
				TempElement += CurrentChar;
			}

			if (SubstringEndChars.Count > 0)
			{
				Log.Warning(string.Format("Improperly formatted config value string! Returning partial results. Missing characters: {0} Config string: {1}", string.Join("", SubstringEndChars), ConfigString));
			}
			else if (TempElement.Length > 0)
			{
				FoundElements.Add(TempElement.Trim());
				TempElement = string.Empty;
			}

			return FoundElements;
		}

		/// <summary>
		/// Helper function to return a Dictionary from a found config value string.
		/// </summary>
		/// <example>
		/// Given a config section:
		/// <code>
		/// [/Script/FortniteGame.FortProfileGo]
		/// ProfileGoScenarios=(Name="BestCase", Position=(X=108298.719,Y=-131650.031,Z=-3136.548))
		/// </code>
		/// The following code will set the value of MyScenarios to a List of one string with a value of '(Name="BestCase", Position=(X=108298.719,Y=-131650.031,Z=-3136.548))'
		/// <code>
		/// GetConfigHierarchy(Context).TryGetValues("/Script/FortniteGame.FortProfileGo", "ProfileGoScenarios", out MyScenarios);
		/// </code>
		/// Using this functions as follows will return a Dictionary with two KeyValuePairs whose values are 'Key:Name Value:"BestCase"' and 'Key:Position Value:(X=108298.719,Y=-131650.031,Z=-3136.548)'
		/// <code>
		/// ParseDictionaryFromConfigString(MyScenarios);
		/// </code>
		/// The function could be called again against returned values to parse further nested Dictionaries such as the FVector represented by the value for 'Position' in the above example.
		/// </example>
		public static Dictionary<string, string> ParseDictionaryFromConfigString(string ConfigString, char KeyValueSeparator = '=')
		{
			Dictionary<string, string> KeyValuePairs = new Dictionary<string, string>(StringComparer.CurrentCultureIgnoreCase);
			List<string> RawPairs = ParseListFromConfigString(ConfigString);

			foreach (string RawPair in RawPairs)
			{
				int SeparatorIndex = RawPair.IndexOf(KeyValueSeparator);
				if (RawPair.Length <= SeparatorIndex + 1)
				{
					Log.Warning(string.Format("Found an element in a config string that was not a KeyValuePair! Skipping this value: {0} From this config string: {1}", RawPair, ConfigString));
					continue;
				}
				string KeyString = RawPair.Substring(0, SeparatorIndex).Trim();
				string ValueString = RawPair.Substring(SeparatorIndex + 1).Trim();
				KeyValuePairs.Add(KeyString, ValueString);
			}

			return KeyValuePairs;
		}
	}

}