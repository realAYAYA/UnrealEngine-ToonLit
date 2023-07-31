// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Linq;
using System.Xml.Linq;

namespace LowLevelTests
{
	public class LowLevelTestsReportParser
	{
		private XDocument ReportDoc;
		public bool IsValid { get; protected set; }

		public LowLevelTestsReportParser(string InLocalReportPath)
		{
			try
			{
				ReportDoc = XDocument.Load(InLocalReportPath);
				IsValid = true;
			}
			catch (Exception LoadEx)
			{
				Log.Error("Encountered error while loading report {0}", LoadEx.ToString());
				IsValid = false;
			}
		}

		public bool HasPassed()
		{
			if (!IsValid)
			{
				return false;
			}

			int NrOverallResultsFailures = -1;
			int NrOverallResultsSuccesses = -1;
			int NrOverallResultsCasesFailures = -1;
			int NrOverallResultsCasesSuccesses = -1;
			XElement OverallResults = ReportDoc.Descendants("OverallResults").FirstOrDefault();
			if (OverallResults != null)
			{
				NrOverallResultsFailures = int.Parse(OverallResults.Attribute("failures").Value);
				NrOverallResultsSuccesses = int.Parse(OverallResults.Attribute("successes").Value);
			}
			XElement OverallResultsCases = ReportDoc.Descendants("OverallResultsCases").FirstOrDefault();
			if (OverallResultsCases != null)
			{
				NrOverallResultsCasesFailures = int.Parse(OverallResultsCases.Attribute("failures").Value);
				NrOverallResultsCasesSuccesses = int.Parse(OverallResultsCases.Attribute("successes").Value);
			}
			
			return NrOverallResultsFailures == 0 && NrOverallResultsCasesFailures == 0 && NrOverallResultsSuccesses > 0 && NrOverallResultsCasesSuccesses > 0;
		}
	}
}
