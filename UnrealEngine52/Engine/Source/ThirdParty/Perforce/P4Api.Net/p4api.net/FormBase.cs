using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace Perforce.P4
{
	/// <summary>
	/// Describes fields and comments in a Perforce specification.
	/// </summary>
	public class FormBase :Dictionary<string, object>
	{
		public Dictionary<string, bool> IsFieldMultiLine { get; internal set; }
		public String Comments { get; set; }

		public FormBase()
		{
			IsFieldMultiLine = new Dictionary<string,bool>();
		}

		#region Tagged Form Data
		/// <summary>
		/// Set the Values Dictionary from tagged output of a Perforce command. 
		/// </summary>
		/// <remarks>
		/// Needed when the object's data dictionary is set after the object
		/// is created using the default constructer.
		/// </remarks>
		/// <param name="data">Object data</param>
		internal void SetValues(TaggedObject data)
		{
			//if (data.ContainsKey("specdef"))
			//{ 
			//    specDescription =  new SpecificationMetaData(data["specdef"]);
			//}
			// map of base key names to max index (so far)
			Dictionary<string, int> baseKeys = new Dictionary<string,int>();

			foreach (String key in data.Keys)
			{
				try
				{
					if (char.IsNumber(key, key.Length - 1))
					{
						// value is part of a list
						string baseKey = key.TrimEnd('0', '1', '2', '3', '4', '5', '6', '7', '8', '9');

						string idxStr = key.Substring(baseKey.Length);
						int idx = -1;
						int.TryParse(idxStr, out idx);

						IsFieldMultiLine[baseKey] = false;

						if (ContainsKey(baseKey) == false)
						{
							this[baseKey] = new List<string>(idx + 1);
						}
						List<string> strList = this[baseKey] as List<string>;
						while (strList.Count <= idx)
						{
							strList.Add(null);
						}
						strList[idx] = data[key];
					}
					else
					{
                        if (data[key].Contains("\r") ||
                            data[key].Contains("\n"))
                        {
                            IsFieldMultiLine[key] = true;
                        }
						this[key] = data[key];
					}
				}
				catch { }; //Extra tag included with TaggedInfoItem data can be ignored
			}
		}
		#endregion

		#region From string data (a spec form)

		/// <summary>
		/// Parse a string specification in the server format into an object.
		/// </summary>
		/// <remarks>
		/// The base implementation parses the generic specification tag::value
		/// format into the underlying dictionary
		/// </remarks>
		/// <param name="spec">String specification</param>
		/// <returns>Success/Failure</returns>
		virtual public bool Parse(String spec)
		{
			String currentTag = String.Empty;
			SimpleList<string> currentValueList = null;

			String[] lines = spec.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
			for (int idx = 0; idx < lines.Length; idx++)
			{
				if (lines[idx][0] == '#') // # comment
				{
					if (Comments == null)
					{
						Comments = lines[idx];
					}
					else
					{
						Comments += "\r\n";
						Comments += lines[idx];
					}
					continue;
				}
				else if (lines[idx][0] == '\t') // tab char so this is part of a value field
				{
					currentValueList.Add(lines[idx].TrimStart('\t'));
				}
				else
				{
					//new tag line
					String line = lines[idx].Trim();
					int colonPos = line.IndexOf(':');
					if (colonPos <= 0) //bad line
						continue;

					// start of a tag
					currentValueList = null;

					currentTag = line.Substring(0, colonPos);
					if (colonPos < line.Length - 1)
					{
						// right of colon is the value
						this[currentTag] = line.Substring(colonPos + 1).Trim();
						IsFieldMultiLine[currentTag] = false;
					}
					else
					{
						IsFieldMultiLine[currentTag] = true;
						currentValueList = new SimpleList<string>();
						this[currentTag] = currentValueList;
					}
				}
			}
			return true;
		}

		#endregion
		/// <summary>
		/// Create a form specification from the fields that make up a form
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			StringBuilder buff = new StringBuilder(2048);

			if (Comments != null)
			{
				buff.AppendLine(Comments);
			}
			foreach (string key in Keys)
			{
                // ignore values that do not go into the spec form
                if (key == "specdef" || key == "specFormatted" || key == "func")
                {
                    continue;
                }

			    if ((IsFieldMultiLine.ContainsKey(key)) && (IsFieldMultiLine[key]))
                    {
                        buff.AppendFormat("{0}:\n", key);

                        if (this[key] is string)
                        {
                            string multilineString = this[key] as string;
                           multilineString= multilineString.TrimEnd('\n', '\r');
                            if (multilineString.Contains("\n"))
                            {
                                multilineString = multilineString.Replace("\r", "");
                                multilineString = multilineString.Replace("\n", "\n\t");
                            }
                            else if (multilineString.Contains("\r"))
                            {
                                multilineString = multilineString.Replace("\r", "\n\t");
                            }
                            buff.AppendFormat("\t{0}\n", (multilineString));
                        }
                        else
                        {
                            IList<string> lines = this[key] as IList<string>;
                            for (int idx = 0; idx < lines.Count; idx++)
                            {
                                buff.AppendFormat("\t{0}\n", lines[idx]);
                            }
                        }
                    }
                    else
                    {
                        buff.AppendFormat("{0}:\t{1}\n", key, this[key] as string);
                    }
			}
			return buff.ToString();
		}
		/// <summary>
		/// Utility function to format a DateTime in the format expected in a spec
		/// </summary>
		/// <param name="dt"></param>
		/// <returns></returns>
		public static String FormatDateTime(DateTime dt)
		{
		    if ((dt != null) && (DateTime.MinValue != dt))
               return dt.ToString("yyyy|MM|dd HH:mm:ss").Replace('|','/');
			return string.Empty;
		}

		/// <summary>
		/// Utility to convert a Unix time (Seconds past midnight 1/1/1970) to a DateTime 
		/// </summary>
		/// <param name="unixTime"></param>
		/// <returns></returns>
		public static DateTime ConvertUnixTime(long unixTime)
		{
			return new DateTime(1970, 1, 1, 0, 0, 0, 0).AddSeconds(unixTime);
		}

		/// <summary>
		/// Utility to convert a Unix time (Seconds past midnight 1/1/1970) to a DateTime 
		/// </summary>
		/// <param name="unixTimeStr">Unix time as a string</param>
		/// <returns></returns>
		public static DateTime ConvertUnixTime(string unixTimeStr)
		{
			long unixTime = 0;
			if (long.TryParse(unixTimeStr, out unixTime))
			{
				return new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc).AddSeconds(unixTime);
			}
			return new DateTime(1970, 1, 1, 0, 0, 0, 0);
		}

        public static bool DSTMismatch(ServerMetaData smd)
        {
            DateTime serverDate = smd.Date;

			string timeZoneId = "GMT Standard Time";
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				timeZoneId = "Europe/London";
			}
            bool UTC_dst = TimeZoneInfo.FindSystemTimeZoneById(timeZoneId).IsDaylightSavingTime(serverDate);
            bool server_dst = serverDate.IsDaylightSavingTime();
            if (server_dst && !UTC_dst)
            {
                return true;
            }
            return false;
        }

        public static DateTime ConvertFromUTC(DateTime dt, string offset, bool DST_mistmatch)
        {
            int idx = offset.IndexOf(" ");
            if (idx >= 0)
            {
                offset = offset.Remove(offset.IndexOf(" "));
            }
            offset = offset.TrimEnd('0');
            int timeDiff;
            bool num = int.TryParse(offset, out timeDiff);

            dt = dt.AddHours(timeDiff);

            bool timestampDST = dt.IsDaylightSavingTime();
           
            if (!timestampDST&&DST_mistmatch)
            {
                dt = dt.AddHours(-1);
            }

            return dt;
        }

		/// <summary>
		/// Utility to properly format multi-line fields in forms
		/// </summary>
		/// <param name="multiline">Multi-line field</param>
		/// <returns></returns>
		public static string FormatMultilineField(string multiline)
		{
			String tmpMultilineStr = String.Empty;
			if (!String.IsNullOrEmpty(multiline))
			{
				tmpMultilineStr = multiline.Replace("\r", "").Trim('\r', '\n');
				tmpMultilineStr = tmpMultilineStr.Replace("\n", "\n\t").Trim();
			}
			return tmpMultilineStr;
		}
	}
}
