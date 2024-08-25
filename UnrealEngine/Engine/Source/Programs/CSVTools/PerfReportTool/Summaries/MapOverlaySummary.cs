// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using CSVStats;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.Versioning;

namespace PerfSummaries
{
	class MapOverlaySummary : Summary
	{
		class MapOverlayEvent
		{
			public MapOverlayEvent(string inName)
			{
				name = inName;
			}
			public MapOverlayEvent(XElement element)
			{
			}
			public string name;
			public string summaryStatName;
			public string shortName;
			public string lineColor;
		};

		class MapOverlay
		{
			public MapOverlay(XElement element, XmlVariableMappings vars)
			{
				positionStatNames[0] = element.GetSafeAttribute<string>(vars, "xStat");
				positionStatNames[1] = element.GetSafeAttribute<string>(vars, "yStat");
				positionStatNames[2] = element.GetSafeAttribute<string>(vars, "zStat");
				summaryStatNamePrefix = element.GetSafeAttribute<string>(vars, "summaryStatNamePrefix"); // unused!
				lineColor = element.GetSafeAttribute<string>(vars, "lineColor", "#ffffff");
				foreach (XElement eventEl in element.Elements("event"))
				{
					MapOverlayEvent ev = new MapOverlayEvent(eventEl.GetRequiredAttribute<string>(vars, "name"));
					ev.shortName = eventEl.GetSafeAttribute<string>(vars, "shortName");
					ev.summaryStatName = eventEl.GetSafeAttribute<string>(vars, "summaryStatName"); // unused!
					ev.lineColor = eventEl.GetSafeAttribute<string>(vars, "lineColor");
					if (eventEl.GetSafeAttribute<bool>(vars, "isStartEvent", false))
					{
						if (startEvent != null)
						{
							throw new Exception("Can't have multiple start events!");
						}
						startEvent = ev;
					}
					events.Add(ev);
				}

			}
			public string[] positionStatNames = new string[3];
			public string summaryStatNamePrefix;
			public MapOverlayEvent startEvent;
			public string lineColor;
			public List<MapOverlayEvent> events = new List<MapOverlayEvent>();
		}

		public MapOverlaySummary(XElement element, XmlVariableMappings vars, string baseXmlDirectory)
		{
			ReadStatsFromXML(element, vars);
			if (stats.Count != 0)
			{
				throw new Exception("<stats> element is not supported");
			}

			sourceImagePath = element.GetSafeAttribute<string>(vars, "sourceImage");
			if (baseXmlDirectory == null)
			{
				throw new Exception("BaseXmlDirectory not specified");
			}
			if (!System.IO.Path.IsPathRooted(sourceImagePath))
			{
				sourceImagePath = System.IO.Path.GetFullPath(System.IO.Path.Combine(baseXmlDirectory, sourceImagePath));
			}

			offsetX = element.GetSafeAttribute<float>(vars, "offsetX", 0.0f);
			offsetY = element.GetSafeAttribute<float>(vars, "offsetY", 0.0f);
			scale = element.GetSafeAttribute<float>(vars, "scale", 1.0f);
			title = element.GetSafeAttribute(vars, "title", "Events");
			destImageFilename = element.GetRequiredAttribute<string>(vars, "destImage");
			imageWidth = element.GetSafeAttribute<float>(vars, "width", 250.0f);
			imageHeight = element.GetSafeAttribute<float>(vars, "height", 250.0f);
			framesPerLineSegment = element.GetSafeAttribute<int>(vars, "framesPerLineSegment", 5);
			lineSplitDistanceThreshold = element.GetSafeAttribute<float>(vars, "lineSplitDistanceThreshold", float.MaxValue);

			foreach (XElement overlayEl in element.Elements("overlay"))
			{
				MapOverlay overlay = new MapOverlay(overlayEl, vars);
				overlays.Add(overlay);
				stats.Add(overlay.positionStatNames[0]);
				stats.Add(overlay.positionStatNames[1]);
				stats.Add(overlay.positionStatNames[2]);
			}
		}
		public MapOverlaySummary() { }

		public override string GetName() { return "mapoverlay"; }

		int toSvgX(float worldX, float worldY)
		{
			float svgX = (worldY * scale + offsetX) * 0.5f + 0.5f;
			svgX *= imageWidth;
			return (int)(svgX + 0.5f);
		}

		int toSvgY(float worldX, float worldY)
		{
			float svgY = 1.0f - (worldX * scale + offsetY) * 0.5f - 0.5f;
			svgY *= imageHeight;
			return (int)(svgY + 0.5f);
		}

		private void CopyAndResizeImage(string sourceImagePath, string destImagePath, int destWidth, int destHeight)
		{
			if (!OperatingSystem.IsWindows())
			{
				Console.WriteLine("CopyAndResizeImage is not supported on this platform!");
				return;
			}
			Console.WriteLine("Downsampling map image.\n  Source: " + sourceImagePath + "\n  Dest  : " + destImagePath);
			using (FileStream fileStream = new FileStream(sourceImagePath, FileMode.Open, FileAccess.Read))
			{
				Console.WriteLine("Reading source image");
				using (var image = Image.FromStream(fileStream))
				{
					Console.WriteLine("Generating downsampled image");
					var thumbnail = image.GetThumbnailImage(destWidth, destHeight, null, IntPtr.Zero);
					using (var destImageStream = new FileStream(destImagePath, FileMode.OpenOrCreate, FileAccess.Write))
					{
						Console.WriteLine("Saving downsampled map image: " + destImageStream);
						thumbnail.Save(destImageStream, ImageFormat.Jpeg);
					}
				}
			}
		}

		public override HtmlSection WriteSummaryData(bool bWriteHtml, CsvStats csvStats, CsvStats csvStatsUnstripped, bool bWriteSummaryCsv, SummaryTableRowData rowData, string htmlFileName)
		{
			HtmlSection htmlSection = null;

			// Output HTML
			if (bWriteHtml)
			{
				htmlSection = new HtmlSection(title, bStartCollapsed);
				string outputDirectory = System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(htmlFileName));
				string outputMapFilename = System.IO.Path.Combine(outputDirectory, destImageFilename);

				// Skip the copy if the output file already exists
				if (!File.Exists(outputMapFilename))
				{
					if (File.Exists(sourceImagePath))
					{
						// Copy the file to the reports folder and reset attributes to ensure it's not readonly if the source is
						//					File.Copy(sourceImagePath, outputMapFilename);
						//					File.SetAttributes(outputMapFilename, FileAttributes.Normal);
						CopyAndResizeImage(sourceImagePath, outputMapFilename, (int)imageWidth, (int)imageHeight);
					}
					else
					{
						Console.WriteLine("[Warning] Can't find source map image: " + sourceImagePath);
					}
				}

				// Check if the file exists in the output directory
				htmlSection.WriteLine("<svg version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' width='" + imageWidth + "' height='" + imageHeight + "'>");
				htmlSection.WriteLine("<image href='" + destImageFilename + "' width='" + imageWidth + "' height='" + imageHeight + "' />");

				// Draw the overlays
				foreach (MapOverlay overlay in overlays)
				{
					StatSamples xStat = csvStats.GetStat(overlay.positionStatNames[0]);
					StatSamples yStat = csvStats.GetStat(overlay.positionStatNames[1]);

					if (xStat == null || yStat == null)
					{
						continue;
					}

					// If a startevent is specified, update the start frame
					int startFrame = 0;
					if (overlay.startEvent != null)
					{
						foreach (CsvEvent ev in csvStats.Events)
						{
							if (CsvStats.DoesSearchStringMatch(ev.Name, overlay.startEvent.name))
							{
								startFrame = ev.Frame;
								break;
							}
						}
					}

					// Make a mapping from frame to map indices
					List<KeyValuePair<int, MapOverlayEvent>> frameEvents = new List<KeyValuePair<int, MapOverlayEvent>>();
					foreach (MapOverlayEvent mapEvent in overlay.events)
					{
						foreach (CsvEvent ev in csvStats.Events)
						{
							if (CsvStats.DoesSearchStringMatch(ev.Name, mapEvent.name))
							{
								frameEvents.Add(new KeyValuePair<int, MapOverlayEvent>(ev.Frame, mapEvent));
							}
						}
					}
					frameEvents.Sort((pair0, pair1) => pair0.Key.CompareTo(pair1.Key));
					int eventIndex = 0;

					// Draw the lines
					string currentLineColor = overlay.lineColor;
					string lineStartTemplate = "<polyline style='fill:none;stroke-width:1.3;stroke:{LINECOLOUR}' points='";
					htmlSection.Write(lineStartTemplate.Replace("{LINECOLOUR}", currentLineColor));
					float adjustedLineSplitDistanceThreshold = lineSplitDistanceThreshold * framesPerLineSegment;
					float oldx = 0;
					float oldy = 0;
					int lastFrameIndex = 0;
					for (int i = startFrame; i < xStat.samples.Count; i += framesPerLineSegment)
					{
						float x = xStat.samples[i];
						float y = yStat.samples[i];
						string lineCoordsStr = toSvgX(x, y) + "," + toSvgY(x, y) + " ";

						// Figure out which event we're up to so we can do color changes
						bool restartLineStrip = false;
						while (eventIndex < frameEvents.Count && lastFrameIndex < frameEvents[eventIndex].Key && i >= frameEvents[eventIndex].Key)
						{
							MapOverlayEvent mapEvent = frameEvents[eventIndex].Value;
							string newLineColor = mapEvent.lineColor != null ? mapEvent.lineColor : overlay.lineColor;
							// If we changed color, restart the line strip
							if (newLineColor != currentLineColor)
							{
								currentLineColor = newLineColor;
								restartLineStrip = true;
							}
							eventIndex++;
						}

						// If the distance between this point and the last is over the threshold, restart the line strip
						float maxManhattanDist = Math.Max(Math.Abs(x - oldx), Math.Abs(y - oldy));
						if (maxManhattanDist > adjustedLineSplitDistanceThreshold)
						{
							restartLineStrip = true;
						}
						else
						{
							htmlSection.Write(lineCoordsStr);
						}

						if (restartLineStrip)
						{
							htmlSection.WriteLine("'/>");
							htmlSection.Write(lineStartTemplate.Replace("{LINECOLOUR}", currentLineColor));
							htmlSection.Write(lineCoordsStr);
						}
						oldx = x;
						oldy = y;
						lastFrameIndex = i;
					}
					htmlSection.WriteLine("'/>");

					// Plot the events 
					float circleRadius = 3;
					string eventColourString = "#ffffff";
					foreach (MapOverlayEvent mapEvent in overlay.events)
					{
						foreach (CsvEvent ev in csvStats.Events)
						{
							if (CsvStats.DoesSearchStringMatch(ev.Name, mapEvent.name))
							{
								string eventText = mapEvent.shortName != null ? mapEvent.shortName : ev.Name;
								float x = xStat.samples[ev.Frame];
								float y = yStat.samples[ev.Frame];
								int svgX = toSvgX(x, y);
								int svgY = toSvgY(x, y);
								htmlSection.Write("<circle cx='" + svgX + "' cy='" + svgY + "' r='" + circleRadius + "' fill='" + eventColourString + "' fill-opacity='1.0'/>");
								htmlSection.WriteLine("<text x='" + (svgX + 5) + "' y='" + svgY + "' text-anchor='left' style='font-family: Verdana;fill: #ffffff; font-size: " + 9 + "px;'>" + eventText + "</text>");
							}
						}
					}
				}

				//htmlSection.WriteLine("<text x='50%' y='" + (imageHeight * 0.05) + "' text-anchor='middle' style='font-family: Verdana;fill: #FFFFFF; stroke: #C0C0C0;  font-size: " + 20 + "px;'>" + title + "</text>");
				htmlSection.WriteLine("</svg>");
			}

			// Output row data
			if (rowData != null)
			{
			}
			return htmlSection;
		}
		public override void PostInit(ReportTypeInfo reportTypeInfo, CsvStats csvStats)
		{
		}
		string title;
		string sourceImagePath;
		float offsetX;
		float offsetY;
		float scale;
		string destImageFilename;
		float imageWidth;
		float imageHeight;
		float lineSplitDistanceThreshold;
		int framesPerLineSegment;

		List<MapOverlay> overlays = new List<MapOverlay>();
	};

}