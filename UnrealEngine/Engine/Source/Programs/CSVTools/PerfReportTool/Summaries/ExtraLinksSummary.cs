// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using System.IO;
using CSVStats;

namespace PerfSummaries
{
	class ExtraLinksSummary : Summary
	{
		class ExtraLink
		{
			public ExtraLink(string fileLine, string inLinkTemplateCsvId)
			{
				string[] Sections = fileLine.Split(',');
				if (Sections.Length != 3)
                {
					throw new Exception("Bad links line format: " + fileLine);
				}
                ReportText = Sections[0];
                SummaryTableText = Sections[1];
				LinkURL = Sections[2];
				if (!LinkURL.StartsWith("http://") && !LinkURL.StartsWith("https://") && !LinkURL.StartsWith("/"))
				{
					LinkTemplateCsvId = inLinkTemplateCsvId;
				}
			}
            public string GetLinkString(bool bIsSummaryTable)
            {
                if (bIsSummaryTable && LinkTemplateCsvId != null)
                {
                    // Output a link template if link templates are enabled and we have a CSV
                    return "{LinkTemplate:" + SummaryTableText.Replace(" ", "") + ":" + LinkTemplateCsvId + "}";
                }
                string Text = bIsSummaryTable ? SummaryTableText : ReportText;
                return "<a href='" + LinkURL + "'>" + Text + "</a>";
            }
            public string ReportText;
			public string SummaryTableText;
			public string LinkURL;
            public string LinkTemplateCsvId;
        };
        public ExtraLinksSummary(XElement element, string baseXmlDirectory, bool bInLinkTemplates=false)
        {
            title = "Links";
            if (element != null)
            {
                title = element.GetSafeAttibute("title", title);
            }
            bLinkTemplates = bInLinkTemplates;
        }

		public ExtraLinksSummary() { }

		public override string GetName() { return "extralinks"; }

		public override void WriteSummaryData(System.IO.StreamWriter htmlFile, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
		{
			List<ExtraLink> links = new List<ExtraLink>();

            string csvId = null;
            if (bLinkTemplates && csvStats.metaData != null)
            {
                csvId = csvStats.metaData.GetValue("csvid", null);
            }


            string csvFilename = csvStats.metaData.GetValue("csvfilename", null);
			if (csvFilename == null)
			{
				Console.WriteLine("Can't find CSV filename for ExtraLinks summary. Skipping");
				return;
			}

			string linksFilename = csvFilename + ".links";
			if (!File.Exists(linksFilename))
			{
				Console.WriteLine("Can't find file " + linksFilename + " for ExtraLinks summary. Skipping");
				return;
			}
			string[] lines = File.ReadAllLines(linksFilename);
			foreach (string line in lines)
			{
				links.Add(new ExtraLink(line, csvId));
			}
			if (links.Count == 0)
			{
				return;
			}

			// Output HTML
			if (htmlFile != null)
			{
				htmlFile.WriteLine("  <h2>" + title + "</h2>");
				htmlFile.WriteLine("  <ul>");
				foreach (ExtraLink link in links)
				{
					htmlFile.WriteLine("  <li>" + link.GetLinkString(false) + "</li>");
				}
				htmlFile.WriteLine("  </ul>");
			}

			// Output summary row data
			if (rowData != null)
			{
				foreach (ExtraLink link in links)
				{
					rowData.Add(SummaryTableElement.Type.SummaryTableMetric, link.ReportText, link.GetLinkString(true), null);
				}
			}
		}
		public override void PostInit(ReportTypeInfo reportTypeInfo, CsvStats csvStats)
		{
        }
		string title;
        bool bLinkTemplates;
	};

}