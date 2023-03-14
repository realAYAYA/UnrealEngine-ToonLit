// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Xml.Linq;
using PerfSummaries;

namespace PerfReportTool
{
	class ReportTypeParams
	{
		public string reportTypeOverride;
		public bool forceReportType;
	}

	class ReportTypeInfo
	{
		public ReportTypeInfo(XElement element, Dictionary<string, XElement> sharedSummaries, string baseXmlDirectory)
		{
			graphs = new List<ReportGraph>();
			summaries = new List<Summary>();
			name = element.Attribute("name").Value;
			title = element.Attribute("title").Value;
			bStripEvents = element.GetSafeAttibute<bool>("stripEvents", true);

			foreach (XElement child in element.Elements())
			{
				if (child.Name == "graph")
				{
					ReportGraph graph = new ReportGraph(child);
					graphs.Add(graph);
				}
				else if (child.Name == "summary" || child.Name == "summaryRef")
				{
					XElement summaryElement = null;
					if (child.Name == "summaryRef")
					{
						summaryElement = sharedSummaries[child.Attribute("name").Value];
					}
					else
					{
						summaryElement = child;
					}
					string summaryType = summaryElement.Attribute("type").Value;
					summaries.Add(SummaryFactory.Create(summaryType, summaryElement, baseXmlDirectory));
				}
				else if (child.Name == "metadataToShow")
				{
					metadataToShowList = child.Value.Split(',');
				}

			}
			ComputeSummaryTableCacheID();
		}

		public string GetSummaryTableCacheID()
		{
			return summaryTableCacheID;
		}

		private void ComputeSummaryTableCacheID()
		{
			StringBuilder sb = new StringBuilder();
			sb.Append("NAME={" + name + "}\n");
			sb.Append("TITLE={" + title + "}\n");
			foreach (Summary summary in summaries)
			{
				sb.Append("SUMMARY={");
				sb.Append("TYPE={" + summary.GetType().ToString() + "}");
				sb.Append("UNSTRIPPED={" + summary.useUnstrippedCsvStats + "}");
				sb.Append("STATS={" + string.Join(",", summary.stats) + "}");
				sb.Append("}");
			}
			summaryTableCacheID = HashHelper.StringToHashStr(sb.ToString(), 16);
		}

		public List<ReportGraph> graphs;
		public List<Summary> summaries;
		public string name;
		public string title;
		public string[] metadataToShowList;
		public string summaryTableCacheID;
		public bool bStripEvents;
	};
}