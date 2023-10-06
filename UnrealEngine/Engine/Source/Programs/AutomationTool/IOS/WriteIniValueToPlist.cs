// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml;
using AutomationTool;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

[Help(@"Pulls a value from an ini file and inserts it into a plist.")]
[Help(@"Note currently only looks at values irrespective of sections!")]
[Help("IniFile=<file>", @"Path to ini file to read from")]
[Help("IniProperty=<name>", @"Name of the ini property to read. E.g. 'Version' for 'Version=12.0'")]
[Help("PlistFile=<file>", @"Path to plist file to update")]
[Help("PlistProperty=<name>", @"Plist property to update. E.g. CFBundleShortVersionString")]
public class WriteIniValueToPlist : BuildCommand
{
	/// <summary>
	/// Path to the ini file to be read
	/// </summary>
	protected string IniFile { get; set; }

	/// <summary>
	/// Ini property to read
	/// </summary>
	protected string IniProperty { get; set; }

	/// <summary>
	/// Path to the plist file to be updated
	/// </summary>
	protected string PlistFile { get; set; }

	/// <summary>
	/// Plist property to update
	/// </summary>
	protected string PlistProperty { get; set; }

	/// <summary>
	/// UAT command entry point
	/// </summary>
	/// <returns></returns>
	public override ExitCode Execute()
	{
		// Temp - wrap everything in an exception handler to avoid taking down Fortnite 
		// builds if something isn't handled :)
		try
		{
			IniFile = ParseParamValue("IniFile", IniFile);
			PlistFile = ParseParamValue("PlistFile", PlistFile);
			IniProperty = ParseParamValue("IniProperty", IniProperty);
			PlistProperty = ParseParamValue("PlistProperty", PlistProperty);

			if (!File.Exists(IniFile))
			{
				throw new AutomationException("Must provide a valid ini file with -inifile");
			}

			if (!File.Exists(PlistFile))
			{
				throw new AutomationException("Must provide a valid plist file with -PlistFile");
			}

			if (string.IsNullOrEmpty(IniProperty))
			{
				throw new AutomationException("Must provide a valid ini property -IniProperty");
			}

			if (string.IsNullOrEmpty(PlistProperty))
			{
				throw new AutomationException("Must provide a valid plist property -PlistProperty");
			}

			// Read the ini file
			string IniFileContents = File.ReadAllText(IniFile);

			string IniPattern = string.Format(@"{0}\s*=(.+)", IniProperty);

			Match IniMatch = Regex.Match(IniFileContents, IniPattern, RegexOptions.IgnoreCase);

			if (IniMatch.Success == false)
			{
				throw new AutomationException("Unable to find ini value for property {0}. (Match pattern = {1})", IniProperty, IniPattern);
			}

			string IniValue = IniMatch.Groups[1].ToString().Trim();

			Logger.LogInformation("Found ini value {IniValue} for property {IniProperty}", IniValue, IniProperty);

			string PlistFileContents = File.ReadAllText(PlistFile);

			string PlistPattern = string.Format(@"<key>{0}</key>\s*<.+?>(.+)<.+?>", PlistProperty);

			Match PlistMatch = Regex.Match(PlistFileContents, PlistPattern, RegexOptions.IgnoreCase | RegexOptions.Multiline);

			if (PlistMatch.Success == false)
			{
				throw new AutomationException("Unable to find plist entry for property {0}. (Match pattern = {1})", PlistProperty, PlistPattern);
			}

			// the whole entry is group0 so replace the value of its current entry with the ini value
			string PListEntry = PlistMatch.Groups[0].ToString();
			string PListValue = PlistMatch.Groups[1].ToString();

			Logger.LogInformation("Found existing value {PListValue} for plist entry {PlistProperty}", PListValue, PlistProperty);

			string NewPlistEntry = PListEntry.Replace(PListValue, IniValue);

			Logger.LogInformation("Set entry {PlistProperty} to {IniValue}", PlistProperty, IniValue);

			PlistFileContents = PlistFileContents.Replace(PListEntry, NewPlistEntry);

			Logger.LogInformation("Saving new plist to {PlistFile}", PlistFile);

			File.WriteAllText(PlistFile, PlistFileContents);

			Logger.LogInformation("Done");
		}
		catch (Exception Ex)
		{
			Logger.LogWarning("Failed to update plist {PlistFile}. {Ex}", PlistFile, Ex);
		}

		return ExitCode.Success;
	}
}
