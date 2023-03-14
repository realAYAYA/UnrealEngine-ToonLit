// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using CSVStats;

namespace PerfSummaries
{
	class BucketSummary : Summary
	{
		public BucketSummary(XElement element, string baseXmlDirectory)
		{
			ReadStatsFromXML(element);
			Title = element.GetSafeAttibute("title", "Untitled Bucket Summary");

			XElement BucketElement = element.Element("buckets");
			bool ReportOutOfRangeDefault = BucketElement.GetSafeAttibute<bool>("reportOutOfRange", false);
			ReportBelowRange = BucketElement.GetSafeAttibute<bool>("reportBelowRange", ReportOutOfRangeDefault);
			ReportAboveRange = BucketElement.GetSafeAttibute<bool>("reportAboveRange", ReportOutOfRangeDefault);

			string[] XmlBuckets = BucketElement.Value.Split(',');
			Buckets = new double[XmlBuckets.Length];
			for (int i = 0; i < XmlBuckets.Length; i++)
			{
				Buckets[i] = Convert.ToDouble(XmlBuckets[i], System.Globalization.CultureInfo.InvariantCulture);
			}

			LowEndColor  = new Colour(1.0f, 1.0f, 1.0f, 1.0f);
			HighEndColor = new Colour(1.0f, 1.0f, 1.0f, 1.0f);
			XElement ColorElement = element.Element("colorDisplay");
			if (ColorElement != null)
			{
				LowEndColor = new Colour(ColorElement.GetSafeAttibute<string>("lowEndColor"));
				HighEndColor = new Colour(ColorElement.GetSafeAttibute<string>("highEndColor"));
			}
		}
		public BucketSummary() { }

		public override string GetName() { return "bucketsummary"; }

		public override void WriteSummaryData(System.IO.StreamWriter htmlFile, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData metadata, string htmlFileName)
		{
			// Only HTML reporting is supported (does not summary table row data)
			if (htmlFile == null)
			{
				return;
			}

			if (Buckets.Length == 0)
			{
				return;
			}

			htmlFile.WriteLine("  <h2>" + Title + "</h2>");
			htmlFile.WriteLine("  <table border='0' style='width:1000'>");
			htmlFile.WriteLine("  <tr><td></td>");

			List<string> Hitches = new List<string>();

			if (ReportBelowRange)
			{
				htmlFile.WriteLine("  <th> <" + Buckets[0].ToString("0") + "</b></td>");
			}

			for (int i = 1; i < Buckets.Length; ++i)
			{
				double Begin = Buckets[i - 1];
				double End = Buckets[i];

				htmlFile.WriteLine("  <th> [" + Begin.ToString("0") + ", " + End.ToString("0") + ")"+ "</b></td>");
			}

			if (ReportAboveRange)
			{
				htmlFile.WriteLine("  <th> >=" + Buckets.Last().ToString("0") + "</b></td>");
			}

			htmlFile.WriteLine("  </tr>");

			foreach (string unitStat in stats)
			{
				string StatToCheck = unitStat.Split('(')[0];
				StatSamples Stats = csvStats.GetStat(StatToCheck.ToLower());
				if (Stats == null)
				{
					continue;
				}

				Hitches.Clear();
				htmlFile.WriteLine("  <tr><td><b>" + StatToCheck + "</b></td>");
				Hitches.Add(StatToCheck);

				int[] BucketCounts = new int[Buckets.Length + 1];
				int TotalSamples = Stats.GetNumSamples();
				TotalSamples -= 2; // First and last frames are not counted in GetCountOfFramesAtOrOverBudget.

				BucketCounts[0] = TotalSamples - Stats.GetCountOfFramesAtOrOverBudget((float)Buckets[0]);
				int SamplesAccountedFor = BucketCounts[0];

				for (int i = 0; i < Buckets.Length - 1; ++i)
				{
					double Begin = Buckets[i];
					double End = Buckets[i + 1];
					int BeginCount = (int)Stats.GetCountOfFramesAtOrOverBudget((float)Buckets[i]);
					int EndCount = (int)Stats.GetCountOfFramesAtOrOverBudget((float)Buckets[i + 1]);
					int BucketCount = BeginCount - EndCount;
					BucketCounts[i + 1] = BucketCount;
					SamplesAccountedFor += BucketCount;
				}
				BucketCounts[BucketCounts.Length - 1] = TotalSamples - SamplesAccountedFor;

				int FirstIndex = ReportBelowRange ? 0 : 1;
				int IndexCount = ReportAboveRange ? BucketCounts.Length : BucketCounts.Length - 1;
				int HighestBucketCount = 0;
				for (int i = FirstIndex; i < IndexCount; ++i)
				{
					HighestBucketCount = Math.Max(HighestBucketCount, BucketCounts[i]);
				}

				Colour White = new Colour(1.0f, 1.0f, 1.0f, 1.0f);
				for (int i = FirstIndex; i < IndexCount; ++i)
				{
					int Count = BucketCounts[i];
					float T = (float)i / (float)(IndexCount - 1);
					Colour Color = Colour.LerpUsingHSV(LowEndColor, HighEndColor, T);
					float Intensity = Count / (float)HighestBucketCount;
					Color = Colour.Lerp(White, Color, Intensity);
					string ColorString = Color.ToHTMLString();

					htmlFile.WriteLine("  <td bgcolor=" + ColorString + ">" + Count.ToString("0") + "</td>");
					Hitches.Add(Count.ToString("0"));
				}

				htmlFile.WriteLine("  </tr>");
			}
			htmlFile.WriteLine("  </table>");
		}

		public double[] Buckets;
		public string Title;
		public bool ReportBelowRange;
		public bool ReportAboveRange;
		public Colour LowEndColor;
		public Colour HighEndColor;
	};

}