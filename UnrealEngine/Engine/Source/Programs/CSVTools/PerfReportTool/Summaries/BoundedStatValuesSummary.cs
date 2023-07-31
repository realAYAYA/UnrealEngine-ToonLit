// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using CSVStats;

namespace PerfSummaries
{
	class BoundedStatValuesSummary : Summary
	{
		class Column
		{
			public string name;
			public string formula;
			public double value;
			public string summaryStatName;
			public string statName;
			public string otherStatName;
			public bool perSecond;
			public bool filterOutZeros;
			public bool applyEndOffset;
			public double multiplier;
			public double threshold;
			public double frameExponent; // Exponent for relative frame time (0-1) in streamingstressmetric formula
			public double statExponent; // Exponent for stat value in streamingstressmetric formula
			public ColourThresholdList colourThresholdList;
		};
		public BoundedStatValuesSummary(XElement element, string baseXmlDirectory)
		{
			ReadStatsFromXML(element);
			if (stats.Count != 0)
			{
				throw new Exception("<stats> element is not supported");
			}

			title = element.GetSafeAttibute("title", "Events");
			beginEvent = element.GetSafeAttibute<string>("beginevent");
			endEvent = element.GetSafeAttibute<string>("endevent");

			endOffsetPercentage = 0.0;
			XAttribute endOffsetAtt = element.Attribute("endoffsetpercent");
			if (endOffsetAtt != null)
			{
				endOffsetPercentage = double.Parse(endOffsetAtt.Value);
			}
			columns = new List<Column>();

			foreach (XElement columnEl in element.Elements("column"))
			{
				Column column = new Column();
				double[] colourThresholds = ReadColourThresholdsXML(columnEl.Element("colourThresholds"));
				if (colourThresholds != null)
				{
					column.colourThresholdList = new ColourThresholdList();
					for (int i = 0; i < colourThresholds.Length; i++)
					{
						column.colourThresholdList.Add(new ThresholdInfo(colourThresholds[i], null));
					}
				}

				XAttribute summaryStatNameAtt = columnEl.Attribute("summaryStatName");
				if (summaryStatNameAtt != null)
				{
					column.summaryStatName = summaryStatNameAtt.Value;
				}
				column.statName = columnEl.Attribute("stat").Value.ToLower();
				if (!stats.Contains(column.statName))
				{
					stats.Add(column.statName);
				}
				column.otherStatName = columnEl.GetSafeAttibute<string>("otherStat", "").ToLower();

				column.name = columnEl.Attribute("name").Value;
				column.formula = columnEl.Attribute("formula").Value.ToLower();
				column.filterOutZeros = columnEl.GetSafeAttibute<bool>("filteroutzeros", false);
				column.perSecond = columnEl.GetSafeAttibute<bool>("persecond", false);
				column.multiplier = columnEl.GetSafeAttibute<double>("multiplier", 1.0);
				column.threshold = columnEl.GetSafeAttibute<double>("threshold", 0.0);
				column.applyEndOffset = columnEl.GetSafeAttibute<bool>("applyEndOffset", true);
				column.frameExponent = columnEl.GetSafeAttibute<double>("frameExponent", 4.0);
				column.statExponent = columnEl.GetSafeAttibute<double>("statExponent", 0.25);
				columns.Add(column);
			}
		}

		public BoundedStatValuesSummary() { }

		public override string GetName() { return "boundedstatvalues"; }

		public override void WriteSummaryData(System.IO.StreamWriter htmlFile, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
		{
			int startFrame = -1;
			int endFrame = int.MaxValue;

			// Find the start and end frames based on the events
			if (beginEvent != null)
			{
				foreach (CsvEvent ev in csvStats.Events)
				{
					if (CsvStats.DoesSearchStringMatch(ev.Name, beginEvent))
					{
						startFrame = ev.Frame;
						break;
					}
				}
				if (startFrame == -1)
				{
					Console.WriteLine("BoundedStatValuesSummary: Begin event " + beginEvent + " was not found");
					return;
				}
			}
			if (endEvent != null)
			{
				foreach (CsvEvent ev in csvStats.Events)
				{
					if (CsvStats.DoesSearchStringMatch(ev.Name, endEvent))
					{
						endFrame = ev.Frame;
						if (endFrame > startFrame)
						{
							break;
						}
					}
				}
				if (endFrame == int.MaxValue)
				{
					Console.WriteLine("BoundedStatValuesSummary: End event " + endEvent + " was not found");
					return;
				}
			}
			if (startFrame >= endFrame)
			{
				Console.WriteLine("Warning: BoundedStatValuesSummary: end event "+ endEvent + " appeared before the start event "+beginEvent);
				return;
			}
			endFrame = Math.Min(endFrame, csvStats.SampleCount - 1);
			startFrame = Math.Max(startFrame, 0);

			// Adjust the end frame based on the specified offset percentage, but cache the old value (some columns may need the unmodified one)
			int endEventFrame = Math.Min(csvStats.SampleCount, endFrame + 1);
			if (endOffsetPercentage > 0.0)
			{
				double multiplier = endOffsetPercentage / 100.0;
				endFrame += (int)((double)(endFrame - startFrame) * multiplier);
			}
			endFrame = Math.Min(csvStats.SampleCount, endFrame + 1);
			StatSamples frameTimeStat = csvStats.GetStat("frametime");
			List<float> frameTimes = frameTimeStat.samples;

			// Filter only columns with stats that exist in the CSV
			List<Column> filteredColumns = new List<Column>();
			foreach (Column col in columns)
			{
				if (csvStats.GetStat(col.statName) != null
					&& (String.IsNullOrWhiteSpace(col.otherStatName) || csvStats.GetStat(col.otherStatName) != null))
				{
					filteredColumns.Add(col);
				}
			}

			// Nothing to report, so bail out!
			if (filteredColumns.Count == 0)
			{
				return;
			}

			// Process the column values
			foreach (Column col in filteredColumns)
			{
				List<float> statValues = csvStats.GetStat(col.statName).samples;
				List<float> otherStatValues = String.IsNullOrWhiteSpace(col.otherStatName) ? null : csvStats.GetStat(col.otherStatName).samples;
				double value = 0.0;
				double totalFrameWeight = 0.0;
				int colEndFrame = col.applyEndOffset ? endFrame : endEventFrame;

				if (col.formula == "average")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (col.filterOutZeros == false || statValues[i] > 0)
						{
							value += statValues[i] * frameTimes[i];
							totalFrameWeight += frameTimes[i];
						}
					}
				}
				else if (col.formula == "maximum")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (col.filterOutZeros == false || statValues[i] > 0)
						{
							value = statValues[i] > value ? statValues[i] : value;
						}
					}

					totalFrameWeight = 1.0;
				}
				else if (col.formula == "minimum")
				{
					value = statValues[startFrame];
					for (int i = startFrame + 1; i < colEndFrame; i++)
					{
						if (col.filterOutZeros == false || statValues[i] > 0)
						{
							value = statValues[i] < value ? statValues[i] : value;
						}
					}

					totalFrameWeight = 1.0;
				}
				else if (col.formula == "percentoverthreshold")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (statValues[i] > col.threshold)
						{
							value += frameTimes[i];
						}
						totalFrameWeight += frameTimes[i];
					}
					value *= 100.0;
				}
				else if (col.formula == "percentunderthreshold")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (statValues[i] < col.threshold)
						{
							value += frameTimes[i];
						}
						totalFrameWeight += frameTimes[i];
					}
					value *= 100.0;
				}
				else if (col.formula == "sum")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						value += statValues[i];
					}

					if (col.perSecond)
					{
						double totalTimeMS = 0.0;
						for (int i = startFrame; i < colEndFrame; i++)
						{
							if (col.filterOutZeros == false || statValues[i] > 0)
							{
								totalTimeMS += frameTimes[i];
							}
						}
						value /= (totalTimeMS / 1000.0);
					}
					totalFrameWeight = 1.0;
				}
				else if (col.formula == "sumwhenotheroverthreshold")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (otherStatValues[i] > col.threshold)
						{
							value += statValues[i];
						}
					}

					if (col.perSecond)
					{
						double totalTimeMS = 0.0;
						for (int i = startFrame; i < colEndFrame; i++)
						{
							if ((col.filterOutZeros == false || statValues[i] > 0) && otherStatValues[i] > col.threshold)
							{
								totalTimeMS += frameTimes[i];
							}
						}
						value /= (totalTimeMS / 1000.0);
					}
					totalFrameWeight = 1.0;
				}
				else if (col.formula == "sumwhenotherunderthreshold")
				{
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (otherStatValues[i] < col.threshold)
						{
							value += statValues[i];
						}
					}

					if (col.perSecond)
					{
						double totalTimeMS = 0.0;
						for (int i = startFrame; i < colEndFrame; i++)
						{
							if ((col.filterOutZeros == false || statValues[i] > 0) && otherStatValues[i] < col.threshold)
							{
								totalTimeMS += frameTimes[i];
							}
						}
						value /= (totalTimeMS / 1000.0);
					}
					totalFrameWeight = 1.0;
				}
				else if (col.formula == "streamingstressmetric")
				{
					// Note: tInc is scaled such that it hits 1.0 on the event frame, regardless of the offset
					double tInc = 1.0 / (double)(endEventFrame - startFrame);
					double t = tInc * 0.5;
					for (int i = startFrame; i < colEndFrame; i++)
					{
						if (col.filterOutZeros == false || statValues[i] > 0)
						{
							// Frame weighting is scaled to heavily favor final frames. Note that t can exceed 1 after the event frame if an offset percentage is specified, so we clamp it
							double frameWeight = Math.Pow(Math.Min(t, 1.0), col.frameExponent) * frameTimes[i];

							// If we're past the end event frame, apply a linear falloff to the weight
							if (i >= endEventFrame)
							{
								double falloff = 1.0 - (double)(i - endEventFrame) / (colEndFrame - endEventFrame);
								frameWeight *= falloff;
							}

							// The frame score takes into account the queue depth, but it's not massively significant
							double frameScore = Math.Pow(statValues[i], col.statExponent);
							value += frameScore * frameWeight;
							totalFrameWeight += frameWeight;
						}
						t += tInc;
					}
				}
				else if (col.formula == "ratio")
				{
					double numerator = 0.0;
					for (int i = startFrame; i < colEndFrame; i++)
					{
						numerator += statValues[i];
					}
					double denominator = 0.0;
					for (int i = startFrame; i < colEndFrame; i++)
					{
						denominator += otherStatValues[i];
					}
					value = numerator / denominator; // TODO: Does the rest of the pipeline handle +/-i infinity?
					totalFrameWeight = 1.0;
				}
				else
				{
					throw new Exception("BoundedStatValuesSummary: unexpected formula " + col.formula);
				}
				value *= col.multiplier;
				col.value = value / totalFrameWeight;
			}

			// Output HTML
			if (htmlFile != null)
			{
				htmlFile.WriteLine("  <h2>" + title + "</h2>");
				htmlFile.WriteLine("  <table border='0' style='width:1400'>");
				htmlFile.WriteLine("  <tr>");
				foreach (Column col in filteredColumns)
				{
					htmlFile.WriteLine("<th>" + col.name + "</th>");
				}
				htmlFile.WriteLine("  </tr>");
				htmlFile.WriteLine("  <tr>");
				foreach (Column col in filteredColumns)
				{
					string bgcolor = "'#ffffff'";
					if (col.colourThresholdList != null)
					{
						bgcolor = col.colourThresholdList.GetColourForValue(col.value);
					}
					htmlFile.WriteLine("<td bgcolor=" + bgcolor + ">" + col.value.ToString("0.00") + "</td>");
				}
				htmlFile.WriteLine("  </tr>");
				htmlFile.WriteLine("  </table>");
			}

			// Output summary table row data
			if (rowData != null)
			{
				foreach (Column col in filteredColumns)
				{
					if (col.summaryStatName != null)
					{
						rowData.Add(SummaryTableElement.Type.SummaryTableMetric, col.summaryStatName, col.value, col.colourThresholdList);
					}
				}
			}
		}
		public override void PostInit(ReportTypeInfo reportTypeInfo, CsvStats csvStats)
		{
		}
		string title;
		string beginEvent;
		string endEvent;
		double endOffsetPercentage;
		List<Column> columns;
	};

}