// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.Stats
{
	[UnrealHeaderTool]
	internal class UhtStatsExporter
	{
		[UhtExporter(Name = "Stats", Description = "Type Stats", Options = UhtExporterOptions.None)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		private static void StatsExporter(IUhtExportFactory factory)
		{
			SortedDictionary<string, int> countByType = new();
			foreach (UhtType type in factory.Session.Packages)
			{
				Collect(countByType, type);
			}

			Log.TraceInformation("Counts by type:");

			foreach (KeyValuePair<string, int> kvp in countByType)
			{
				Log.TraceInformation($"{kvp.Key} {kvp.Value}");
			}
			Log.TraceInformation("");
		}

		private static void Collect(SortedDictionary<string, int> countByType, UhtType type)
		{
			if (countByType.TryGetValue(type.EngineClassName, out int count))
			{
				countByType[type.EngineClassName] = count + 1;
			}
			else
			{
				countByType[type.EngineClassName] = 1;
			}

			foreach (UhtType child in type.Children)
			{
				Collect(countByType, child);
			}
		}
	}
}
