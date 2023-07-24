// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using CSVStats;
using PerfReportTool;
using System.Reflection;
using System.Drawing;

namespace PerfSummaries
{
	class SummaryFactory
	{
		public static void Init()
		{
			summaryNameLookup = new Dictionary<string, Type>();
			Assembly assembly = typeof(SummaryFactory).GetTypeInfo().Assembly;
			foreach (Type type in assembly.GetTypes())
			{
				if (type.IsSubclassOf(typeof(Summary)))
				{
					Summary instance = (Summary)Activator.CreateInstance(type);
					summaryNameLookup.Add(instance.GetName(), type);
				}
			}
		}

		public static Summary Create(string summaryTypeName, XElement summaryXmlElement, string baseXmlDirectory)
		{
			if ( summaryNameLookup.TryGetValue(summaryTypeName, out System.Type summaryType ) )
			{
				object[] constructArgs = new object[2] { summaryXmlElement, baseXmlDirectory };
				return (Summary)Activator.CreateInstance(summaryType, constructArgs);
			}
			throw new Exception("Summary type " + summaryType + " not found!");
		}

		static Dictionary<string, System.Type> summaryNameLookup = null;
	};

	abstract class Summary
	{

		public class CaptureRange
        {
            public string name;
            public string startEvent;
            public string endEvent;
            public bool includeFirstFrame;
            public bool includeLastFrame;
            public CaptureRange(string inName, string start, string end)
            {
                name = inName;
                startEvent = start;
                endEvent = end;
                includeFirstFrame = false;
                includeLastFrame = false;
            }
        }
        public class CaptureData
        {
            public int startIndex;
            public int endIndex;
            public List<float> Frames;
            public CaptureData(int start, int end, List<float> inFrames)
            {
                startIndex = start;
                endIndex = end;
                Frames = inFrames;
            }
        }

        public Summary()
        {
            stats = new List<string>();
            captures = new List<CaptureRange>();
            StatThresholds = new Dictionary<string, ColourThresholdList>();

        }
        public virtual void WriteSummaryData(System.IO.StreamWriter htmlFile, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
        { }

        public virtual void PostInit(ReportTypeInfo reportTypeInfo, CsvStats csvStats)
        {
			// Resolve wildcards and remove duplicates
			stats = csvStats.GetStatNamesMatchingStringList(stats.ToArray());
		}

		public abstract string GetName();

        public void ReadStatsFromXML(XElement element)
        {
			useUnstrippedCsvStats = element.GetSafeAttibute<bool>("useUnstrippedCsvStats", false);
			XElement statsElement = element.Element("stats");
			if (statsElement != null)
			{
				stats = statsElement.Value.Split(',').ToList();
			}
            foreach (XElement child in element.Elements())
            {
                if (child.Name == "capture")
                {
                    string captureName = child.Attribute("name").Value;
                    string captureStart = child.Attribute("startEvent").Value;
                    string captureEnd = child.Attribute("endEvent").Value;
                    bool incFirstFrame = Convert.ToBoolean(child.Attribute("includeFirstFrame").Value);
                    bool incLastFrame = Convert.ToBoolean(child.Attribute("includeLastFrame").Value);
                    CaptureRange newRange = new CaptureRange(captureName, captureStart, captureEnd);
                    newRange.includeFirstFrame = incFirstFrame;
                    newRange.includeLastFrame = incLastFrame;
                    captures.Add(newRange);
                }
                else if (child.Name == "colourThresholds")
                {
                    if (child.Attribute("stat") == null)
                    {
                        continue;
                    }
                    string statName = child.Attribute("stat").Value;
                    string[] hitchThresholdsStrList = child.Value.Split(',');
					ColourThresholdList HitchThresholds = new ColourThresholdList();
					for (int i = 0; i < hitchThresholdsStrList.Length; i++)
                    {
						string hitchThresholdStr = hitchThresholdsStrList[i];
						double thresholdValue = 0.0;
						string hitchThresholdNumStr = hitchThresholdStr;
						Colour thresholdColour = null;

						int openBracketIndex = hitchThresholdStr.IndexOf('(');
						if (openBracketIndex != -1 )
						{
							hitchThresholdNumStr = hitchThresholdStr.Substring(0, openBracketIndex);
							int closeBracketIndex = hitchThresholdStr.IndexOf(')');
							if (closeBracketIndex > openBracketIndex)
							{
								string colourString = hitchThresholdStr.Substring(openBracketIndex+1, closeBracketIndex - openBracketIndex-1);
								thresholdColour = new Colour(colourString);
							}
						}
						thresholdValue = Convert.ToDouble(hitchThresholdNumStr, System.Globalization.CultureInfo.InvariantCulture);

						HitchThresholds.Add(new ThresholdInfo(thresholdValue, thresholdColour));
                    }
                    if (HitchThresholds.Count == 4)
                    {
                        StatThresholds.Add(statName, HitchThresholds);
                    }
                }
            }
        }
        public CaptureData GetFramesForCapture(CaptureRange inCapture, List<float> FrameTimes, List<CsvEvent> EventsCaptured)
        {
            List<float> ReturnFrames = new List<float>();
            int startFrame = -1;
            int endFrame = FrameTimes.Count;
            for (int i = 0; i < EventsCaptured.Count; i++)
            {
                if (startFrame < 0 && EventsCaptured[i].Name.ToLower().Contains(inCapture.startEvent.ToLower()))
                {
                    startFrame = EventsCaptured[i].Frame;
                    if (!inCapture.includeFirstFrame)
                    {
                        startFrame++;
                    }
                }
                else if (endFrame >= FrameTimes.Count && EventsCaptured[i].Name.ToLower().Contains(inCapture.endEvent.ToLower()))
                {
                    endFrame = EventsCaptured[i].Frame;
                    if (!inCapture.includeLastFrame)
                    {
                        endFrame--;
                    }
                }
            }
            if (startFrame == -1 || endFrame == FrameTimes.Count || endFrame < startFrame)
            {
                return null;
            }
            ReturnFrames = FrameTimes.GetRange(startFrame, (endFrame - startFrame));
            CaptureData CaptureToUse = new CaptureData(startFrame, endFrame, ReturnFrames);
            return CaptureToUse;
        }

        public string[] GetUniqueStatNames()
        {
            HashSet<string> uniqueStats = new HashSet<string>();
            foreach (string stat in stats)
            {
                if (!uniqueStats.Contains(stat))
                {
                    uniqueStats.Add(stat);
                }
            }
            return uniqueStats.ToArray();
        }

		protected ColourThresholdList ReadColourThresholdListXML(XElement colourThresholdEl)
		{
			return ColourThresholdList.ReadColourThresholdListXML(colourThresholdEl);
		}

		protected double [] ReadColourThresholdsXML(XElement colourThresholdEl)
		{
			return ColourThresholdList.ReadColourThresholdsXML(colourThresholdEl);
		}

        public string GetStatThresholdColour(string StatToUse, double value)
        {
			ColourThresholdList Thresholds = GetStatColourThresholdList(StatToUse);
			if (Thresholds != null)
            {
				return Thresholds.GetColourForValue(value);
            }
			return "'#ffffff'";
        }

		public ColourThresholdList GetStatColourThresholdList(string StatToUse)
		{
			if (StatThresholds.ContainsKey(StatToUse))
			{
				return StatThresholds[StatToUse];
			}
			return null;
		}

        public List<CaptureRange> captures;
        public List<string> stats;
        public Dictionary<string, ColourThresholdList> StatThresholds;
		public bool useUnstrippedCsvStats;
    };










}
