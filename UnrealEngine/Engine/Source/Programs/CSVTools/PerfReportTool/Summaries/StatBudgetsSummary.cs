// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using CSVStats;

/*
This summary sums up multiple CsvStat averages and emits the total as a SummaryTableMetric named metricName (if specified). 
Budget colors can optionally be applied to individual stats and/or the total via colourThresholds and colourThresholdsTotal nodes
If emitStatMetrics is enabled, each stat will emit a SummaryTableMetric, named metricName/strippedStatName (with the same value as the existing CsvStatAverage element if it exists)
Budget coloring is applied to existing CsvStatAverage elements.
The budget for a given stat is assumed to be the first element of its colourThresholds. This value is displayed in the report

Example:
	<summary type="statBudgets" metricName="GTGameplay" title="Gamethread - Gameplay Budgets" hideStatPrefix="Exclusive/GameThread/" emitStatMetrics="1">
		<colourThresholdsTotal>3,4,5,6</colourThresholdsTotal>
		<colourThresholds stat="Exclusive/GameThread/Buildings">1,2,3,4</colourThresholds>
		<stats>Exclusive/GameThread/Buildings,Exclusive/GameThread/Camera,Exclusive/GameThread/Curie,Exclusive/GameThread/Network*,Exclusive/GameThread/PlayerController*,Exclusive/GameThread/WorldTickMisc,Exclusive/GameThread/FlushLatentActions,Exclusive/GameThread/Tickables,Exclusive/GameThread/SyncBodies,Exclusive/GameThread/CharPhys*,Exclusive/GameThread/Character*,Exclusive/GameThread/FortPawnTickSubsystem,Exclusive/GameThread/SignificanceManager,exclusive/gamethread/vehicle*,Exclusive/GameThread/AbilityTasks,Exclusive/GameThread/Actor*,Exclusive/GameThread/HandleRPC,Exclusive/GameThread/RepNotifies,Exclusive/GameThread/TickActors,Exclusive/GameThread/Pickups,Exclusive/GameThread/ProjectileMovement,Exclusive/GameThread/TimelineComponent,Exclusive/GameThread/TimerManager,Exclusive/GameThread/FortTrainManager</stats>
	</summary>
*/

namespace PerfSummaries
{
	class StatInfo
	{
		public StatInfo( StatSamples inCsvStat )
		{
			name = inCsvStat.Name;
			averageValue = inCsvStat.average;
			csvStat = inCsvStat;
		}
		public StatInfo(string inName, double inAverageValue ) 
		{
			name = inName;
			averageValue = inAverageValue;
		}

		public string name;
		public double averageValue;
		public StatSamples csvStat = null;
		public ColourThresholdList colorThresholds = null;
	};

	class StatBudgetsSummary : Summary
	{
		public StatBudgetsSummary(XElement element, XmlVariableMappings vars, string baseXmlDirectory)
		{
			ReadStatsFromXML(element, vars);

			colorThresholdsTotal = ReadColourThresholdListXML(element.Element("colourThresholdsTotal"), vars);
			foreach (XElement child in element.Elements("colourThresholds"))
			{
				ColourThresholdList colorThresholds = ReadColourThresholdListXML(child, vars);
				string statName = child.GetRequiredAttribute<string>(vars, "stat");
				statColorThresholds.Add(statName, colorThresholds);
			}

			metricName = element.GetSafeAttribute<string>(vars, "metricName");
			title = element.GetSafeAttribute<string>(vars, "title", "Budgets summary");
			stripStatPrefix = element.GetSafeAttribute<string>(vars, "stripStatPrefix");
			emitStatMetrics = element.GetSafeAttribute<bool>(vars, "emitStatMetrics", false);
			showTotal = element.GetSafeAttribute<bool>(vars, "showTotal", true);
			totalStatName = element.GetSafeAttribute<string>(vars, "totalStat");

			if ( emitStatMetrics && metricName == null )
			{
				throw new Exception("StatBudgetsSummary error: metricName must be specified if emitStatMetrics is enabled. XML element: " + element.ToString());
			}
		}

		public StatBudgetsSummary() { }

		public override string GetName() { return "statBudgets"; }

		string StripStatPrefix(string statName)
		{
			if (stripStatPrefix != null)
			{
				if (statName.ToLower().StartsWith(stripStatPrefix.ToLower()))
				{
					return statName.Substring(stripStatPrefix.Length);
				}
			}
			return statName;
		}

		public override HtmlSection WriteSummaryData(bool bWriteHtml, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
		{
			HtmlSection htmlSection = null;
			// Find all referenced stats/metadata and sum them
			double totalValue = 0.0;
			List<StatInfo> statInfoList = new List<StatInfo>();
			StatInfo totalStatInfo = null;
			foreach (string statName in stats)
			{
				StatSamples csvStat = csvStats.GetStat(statName);
				if (csvStat == null)
				{
					continue;
				}
				StatInfo statInfo = new StatInfo(csvStat);
				if (statColorThresholds.TryGetValue(statName, out ColourThresholdList colorThresholds))
				{
					statInfo.colorThresholds = colorThresholds;
				}
				if (totalStatName != null && totalStatName.ToLower() == statName.ToLower())
				{
					totalStatInfo = statInfo;
					totalStatInfo.name = "<b>Total</b> (" + StripStatPrefix(statName) + ")";
				}
				else
				{
					statInfoList.Add(statInfo);
					totalValue += statInfo.averageValue;
				}
			}

			// Sort largest to smallest
			statInfoList.Sort((a, b) => b.averageValue.CompareTo(a.averageValue));

			if (totalStatInfo != null)
			{
				// Add an "other" stat if the provided total stat is greater than the computed total
				double otherValue = totalStatInfo.averageValue - totalValue;
				if (otherValue > 0.0001)
				{
					StatInfo otherStatInfo = new StatInfo("<i>Other</i>", otherValue);
					totalStatInfo.colorThresholds = colorThresholdsTotal;
					statInfoList.Add(otherStatInfo);
				}
			}
			else
			{
				totalStatInfo = new StatInfo("<b>Total</b>", totalValue);
				totalStatInfo.colorThresholds = colorThresholdsTotal;
			}
			statInfoList.Add(totalStatInfo);

			if (rowData != null)
			{
				// If we have rowData then apply thresholds to the CsvStatAverage elements (and ensure they exist)
				foreach (StatInfo statInfo in statInfoList)
				{
					if ( statInfo.csvStat != null )
					{
						// Do we have an existing CsvStatAverage metric already?
						SummaryTableElement element = rowData.Get(statInfo.csvStat.Name.ToLower());

						if (element == null)
						{
							throw new Exception("Stat not found: "+ statInfo.csvStat.Name + " in budget summary "+title);
						}

						// Emit the stat metric if requested
						if (emitStatMetrics)
						{
							string shortStatName = StripStatPrefix(statInfo.name);
							string statMetricName = metricName + "/" + shortStatName;
							rowData.Add(SummaryTableElement.Type.SummaryTableMetric, statMetricName, statInfo.averageValue, statInfo.colorThresholds);
						}

						// Update the color color threshold to the CSV stat if specified
						if (statInfo.colorThresholds != null)
						{
							element.colorThresholdList = statInfo.colorThresholds;
						}
					}
				}

				// Output the total metric, if specified
				if (metricName != null)
				{
					rowData.Add(SummaryTableElement.Type.SummaryTableMetric, metricName, totalStatInfo.averageValue, totalStatInfo.colorThresholds);
				}
			}

			// Output HTML
			if (bWriteHtml)
			{
				htmlSection = new HtmlSection(title, bStartCollapsed);
				htmlSection.WriteLine("  <table border='0' style='width:400'>");
				htmlSection.WriteLine("  <tr><th>Stat</th><th>Value (Avg)</th><th>Budget</th></tr>");
				foreach (StatInfo statInfo in statInfoList)
				{
					if (showTotal == false && statInfo == totalStatInfo)
					{
						continue;
					}

					string bgcolor = "'#ffffff'";
					string budgetStr = "";
					if (statInfo.colorThresholds != null)
					{
						bgcolor = statInfo.colorThresholds.GetColourForValue(statInfo.averageValue);
						if (statInfo.colorThresholds.Thresholds.Count > 0)
						{
							budgetStr = statInfo.colorThresholds.Thresholds.First().value.ToString("0.00");
						}						
					}
					string displayName = statInfo.name;
					if (statInfo.csvStat != null)
					{
						displayName = StripStatPrefix(statInfo.name);
					}
					htmlSection.WriteLine("  <tr><td>" + displayName + "</td><td bgcolor=" + bgcolor + ">" + statInfo.averageValue.ToString("0.00") + "</td><td>" + budgetStr + "</td></tr>");
				}
				htmlSection.WriteLine("  </table>");
			}
			return htmlSection;
		}

		ColourThresholdList colorThresholdsTotal = null;
		Dictionary<string, ColourThresholdList> statColorThresholds = new Dictionary<string, ColourThresholdList>();

		string title = "";
		string metricName = null; // This is the output metric name used for the total, and also a prefix
		string stripStatPrefix = null; // Specifies a stat prefix to strip (applies to metric names and report HTML)
		string totalStatName = null; // If a total is specified then we'll use that if it's greater than the summed total. An "other" value stat will be displayed to show the difference
		bool emitStatMetrics = false; // Whether to emit a separate summaryTableMetric for each stat. The name of each metric will be metricName/strippedStatName 
		bool showTotal = true; // Whether to display the total in the detailed report 
	};

}