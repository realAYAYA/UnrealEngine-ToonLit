// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.IO;
using System.Text;

namespace Gauntlet
{
	public static class Horde
	{

		static public bool IsHordeJob
		{
			get
			{
				return !string.IsNullOrEmpty(JobId);
			}
		}

		static public string JobId
		{
			get
			{
				return Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			}
		}

		static public string StepId
		{
			get
			{
				return Environment.GetEnvironmentVariable("UE_HORDE_STEPID");
			}
		}

		public static void GenerateSummary()
		{
			try
			{
				if (!IsHordeJob)
				{
					return;
				}

				string LogFolder = CommandUtils.CmdEnv.LogFolder;

				string MarkdownFilename = "GauntletStepDetails.md";

				// file:// is attractive here, and file://\\computer\share\folder\file.txt does work ok in Chrome, but it does not Firefox
				// so, lacking a uniform way to link to a network filesystem, just put the path here as static text and escaped for markdown
				string Markdown = System.Text.RegularExpressions.Regex.Replace($"Gauntlet Artifacts: {Globals.LogDir}", @"([|\\*\+])", @"\$1");

				File.WriteAllText(Path.Combine(LogFolder, MarkdownFilename), Markdown);

				StringBuilder Builder = new StringBuilder();

				Builder.Append("{\n");

				Builder.Append("\"scope\": \"Step\",\n");
				Builder.Append("\"name\": \"Gauntlet Step Details\",\n");
				Builder.Append("\"placement\": \"Summary\",\n");
				Builder.AppendFormat("\"fileName\": \"{0}\"\n", MarkdownFilename);

				Builder.Append("}\n");

				File.WriteAllText(Path.Combine(LogFolder, "GauntletStepDetails.report.json"), Builder.ToString());

				/*
				using (JsonWriter Writer = new JsonWriter(new FileReference(Path.Combine(LogFolder, "GauntletStepDetails.report.json"))))
				{
					Writer.WriteObjectStart();

					Writer.WriteValue("scope", "Step");
					Writer.WriteValue("name", "Gauntlet Step Details");
					Writer.WriteValue("placement", "Summary");
					Writer.WriteValue("fileName", MarkdownFilename);

					Writer.WriteObjectEnd();
				}
				*/
			}
			catch (Exception Ex)
			{
				Log.Info("Exception while generating Horde summary\n{0}\n", Ex.Message);
			}
		}

	}
}
