// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using CSVStats;

namespace PerfSummaries
{
	class EventSummary : Summary
	{
		public EventSummary(XElement element, string baseXmlDirectory)
		{
			title = element.GetSafeAttibute("title", "Events");
			summaryStatName = element.Attribute("summaryStatName").Value;
			events = element.Element("events").Value.Split(',');
			colourThresholds = ReadColourThresholdsXML(element.Element("colourThresholds"));
		}

		public EventSummary() { }

		public override string GetName() { return "event"; }

		public override void WriteSummaryData(System.IO.StreamWriter htmlFile, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
		{
			Dictionary<string, int> eventCountsDict = new Dictionary<string, int>();
			int eventCount = 0;
			foreach (CsvEvent ev in csvStats.Events)
			{
				foreach (string eventName in events)
				{
					if (CsvStats.DoesSearchStringMatch(ev.Name, eventName))
					{
						int len = eventName.Length;
						if (eventName.EndsWith("*"))
						{
							len--;
						}
						string eventContent = ev.Name.Substring(len).Trim();
						if (eventCountsDict.ContainsKey(eventContent))
						{
							eventCountsDict[eventContent]++;
						}
						else
						{
							eventCountsDict.Add(eventContent, 1);
						}
						eventCount++;
					}
				}
			}

			// Output HTML
			if (htmlFile != null && eventCountsDict.Count > 0)
			{
				htmlFile.WriteLine("  <h2>" + title + "</h2>");
				htmlFile.WriteLine("  <table border='0' style='width:1200'>");
				htmlFile.WriteLine("  <tr><th>Name</th><th><b>Count</th></tr>");
				foreach (KeyValuePair<string, int> pair in eventCountsDict.ToList())
				{
					htmlFile.WriteLine("  <tr><td>" + pair.Key + "</td><td>" + pair.Value + "</td></tr>");
				}
				htmlFile.WriteLine("  </table>");
			}

			// Output summary table row data
			if (rowData != null)
			{

				ColourThresholdList thresholdList = null;

				if (colourThresholds != null)
				{
					thresholdList = new ColourThresholdList();
					for (int i = 0; i < colourThresholds.Length; i++)
					{
						thresholdList.Add(new ThresholdInfo(colourThresholds[i]));
					}
				}
				rowData.Add(SummaryTableElement.Type.SummaryTableMetric, summaryStatName, (double)eventCount, thresholdList);
			}
		}
		public override void PostInit(ReportTypeInfo reportTypeInfo, CsvStats csvStats)
		{
		}
		string[] events;
		double[] colourThresholds;
		string title;
		string summaryStatName;
	};

}