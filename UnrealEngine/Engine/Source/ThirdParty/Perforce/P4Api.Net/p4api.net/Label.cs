/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: Label.cs
 *
 * Author(s)	: wjb, dbb
 *
 * Description	: Class used to abstract a label in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// A label specification in a Perforce repository. 
	/// </summary>
    public class Label
    {
        public Label()
        {
        }

        public Label
            (
            string id,
            string owner,
            DateTime update,
            DateTime access,
            string description,
            bool locked,
            string revision,
            string serverid,
            ViewMap viewmap,
            FormSpec spec,
            string options
            )
        {
            Id = id;
            Owner = owner;
            Update = update;
            Access = access;
            Description = description;
            Locked = locked;
            Revision = revision;
            ServerId = serverid;
            ViewMap = viewmap;
            Spec = spec;
#pragma warning disable 618
            Options = options;
#pragma warning restore 618
        }

        private FormBase _baseForm;

        #region properties

        public string Id { get; set; }
        public string Owner { get; set; }
        public DateTime Update { get; set; }
        public DateTime Access { get; set; }
        public string Description { get; set; }
		public bool Locked { get; set; }
		private bool _autoreload { get; set; }
		public bool Autoreload 
		{
			get { return _autoreload; }
			set
			{
				if (value)
				{
					IncludeAutoreloadOption = true;
				}
				_autoreload = value;
			}
		}
		public bool IncludeAutoreloadOption { get; set; }
		public string Revision { get; set; }
        public string ServerId { get; set; }
        public ViewMap ViewMap { get; set; }
        public FormSpec Spec { get; set; }

	    [Obsolete("Use Locked Property")]
	    public string Options
	    {
	        get
	        {
				string value = Locked ? "locked" : "unlocked";
				if (IncludeAutoreloadOption)
				{
					if (Autoreload)
					{
						value += " autoreload";
					}
					else
					{
						value += " noautoreload";
					}
				}
				return value;
	        }
            set
            {
                Locked = (value.Contains("unlocked")) == false;
				IncludeAutoreloadOption = value.Contains("autoreload");
				Autoreload = IncludeAutoreloadOption && (value.Contains("noautoreload") == false);
            }
	    }

        #endregion
        #region fromTaggedOutput
        /// <summary>
        /// Read the fields from the tagged output of a label command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'label' command</param>
        public void FromLabelCmdTaggedOutput(TaggedObject objectInfo)
        {
            _baseForm = new FormBase();

            _baseForm.SetValues(objectInfo);

            if (objectInfo.ContainsKey("Label"))
                Id = objectInfo["Label"];

            if (objectInfo.ContainsKey("Owner"))
                Owner = objectInfo["Owner"];

            if (objectInfo.ContainsKey("Update"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(objectInfo["Update"], out v);
                Update = v;
            }

            if (objectInfo.ContainsKey("Access"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(objectInfo["Access"], out v);
                Access = v;
            }

            if (objectInfo.ContainsKey("Description"))
                Description = objectInfo["Description"];

            if (objectInfo.ContainsKey("Revision"))
                Revision = objectInfo["Revision"];

            if (objectInfo.ContainsKey("ServerID"))
                Revision = objectInfo["ServerID"];

            if (objectInfo.ContainsKey("Options"))
            {
#pragma warning disable 618
               Options = objectInfo["Options"] as string;
#pragma warning restore 618
            }
            else
                Locked = false;

            int idx = 0;
            string key = String.Format("View{0}", idx);
            if (objectInfo.ContainsKey(key))
            {
                ViewMap = new ViewMap();
                while (objectInfo.ContainsKey(key))
                {
                    ViewMap.Add(objectInfo[key]);
                    idx++;
                    key = String.Format("View{0}", idx);
                }
            }
            else
            {
                ViewMap = null;
            }
        }
        #endregion

        #region label spec support
        /// <summary>
        /// Parse the fields from a label specification 
        /// </summary>
        /// <param name="spec">Text of the label specification in server format</param>
        /// <returns></returns>
        public bool Parse(String spec)
        {
            _baseForm = new FormBase();

            _baseForm.Parse(spec); // parse the values into the underlying dictionary

            if (_baseForm.ContainsKey("Label"))
            {
                Id = _baseForm["Label"] as string;
            }

            if (_baseForm.ContainsKey("Owner"))
            {
                Owner = _baseForm["Owner"] as string;
            }

            if (_baseForm.ContainsKey("Update"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(_baseForm["Update"] as string, out v);
                Update = v;
            }

            if (_baseForm.ContainsKey("Access"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(_baseForm["Access"] as string, out v);
                Access = v;
            }

			if (_baseForm.ContainsKey("Description"))
			{
				object d = _baseForm["Description"];
				if (d is string)
				{
					Description = _baseForm["Description"] as string;
				}
				else if (d is string[])
				{
					string[] a = d as string[];
					Description = string.Empty;
					for (int idx = 0; idx < a.Length; idx++)
					{
						if (idx > 0)
						{
							Description += "\r\n";
						}
						Description += a[idx];
					}
				}
				else if (d is IList<string>)
				{
					IList<string> l = d as IList<string>;
					Description = string.Empty;
					for (int idx = 0; idx < l.Count; idx++)
					{
						if (idx > 0)
						{
							Description += "\r\n";
						}
						Description += l[idx];
					}
				}
                else if (d is SimpleList<string>)
                {
                    SimpleList<string> l = d as SimpleList<string>;
                    Description = string.Empty;
                    SimpleListItem<string> current = l.Head;
                    bool addCRLF = false;
                    while (current != null)
                    {
                        if (addCRLF)
                        {
                            Description += "\r\n";
                        }
                        else
                        {
                            addCRLF = true;
                        }
                        Description += current.Item;
                        current = current.Next;
                    }
                }
            }

            if (_baseForm.ContainsKey("Options"))
            {
#pragma warning disable 618
               Options = _baseForm["Options"] as string;
#pragma warning restore 618
            }

            if (_baseForm.ContainsKey("Revision"))
            {
                Revision = _baseForm["Revision"] as string;
            }

            if (_baseForm.ContainsKey("ServerID"))
            {
                Revision = _baseForm["ServerID"] as string;
            }

            if (_baseForm.ContainsKey("View"))
            {
                if (_baseForm["View"] is IList<string>)
                {
                    IList<string> lines = _baseForm["View"] as IList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
                if (_baseForm["View"] is SimpleList<string>)
                {
                    SimpleList<string> lines = _baseForm["View"] as SimpleList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
            }
            return true;
        }

        /// <summary>
        /// Format of a label specification used to save a label to the server
        /// </summary>
        private static String LabelFormat =
                                                    "Label:\t{0}\r\n" +
                                                    "\r\n" +
                                                    "Update:\t{1}\r\n" +
                                                    "\r\n" +
                                                    "Access:\t{2}\r\n" +
                                                    "\r\n" +
                                                    "Owner:\t{3}\r\n" +
                                                    "\r\n" +
                                                    "Description:\r\n\t{4}\r\n" +
                                                    "\r\n" +
                                                    "Options:\t{5}\r\n" +
                                                    "\r\n" +
                                                    "{6}" +
                                                    "{7}" +
                                                    "View:\r\n\t{8}\r\n";


        /// <summary>
        /// Convert to specification in server format
        /// </summary>
        /// <returns></returns>
        override public String ToString()
        {
            String viewStr = String.Empty;
            if (ViewMap != null)
                viewStr = ViewMap.ToString().Replace("\r\n", "\r\n\t").Trim();
            String OptionsStr = string.Empty;
			if (Locked)
			{
				OptionsStr = "locked";
			}
			else
			{
				OptionsStr = "unlocked";
			}

			if (IncludeAutoreloadOption)
			{
				if (Autoreload)
				{
					OptionsStr += " autoreload";
				}
				else
				{
					OptionsStr += " noautoreload";
				}
			}
            String revStr = String.Empty;
            if (Revision != null)
            {
                revStr = string.Format("Revision:\t{0}\r\n\r\n", Revision);
            }
            String serveridStr = String.Empty;
            if (ServerId != null)
            {
                serveridStr = string.Format("ServerID:\t{0}\r\n\r\n", ServerId);
            }
            String value = String.Format(LabelFormat, Id,
                FormBase.FormatDateTime(Update), FormBase.FormatDateTime(Access),
                Owner, Description, OptionsStr, revStr, serveridStr,viewStr);
            return value;
        }
        #endregion

        /// <summary>
        /// Read the fields from the tagged output of a labels command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'labels' command</param>
        /// <param name="offset">Date handling</param>
        /// <param name="dst_mismatch">DST</param>
        public void FromLabelsCmdTaggedOutput(TaggedObject objectInfo, string offset, bool dst_mismatch)
        {
            _baseForm = new FormBase();

            _baseForm.SetValues(objectInfo);

            if (objectInfo.ContainsKey("label"))
                Id = objectInfo["label"];

            if (objectInfo.ContainsKey("Owner"))
                Owner = objectInfo["Owner"];

            if (objectInfo.ContainsKey("Access"))
            {
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Access"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);
                Access = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
            }
           
            if (objectInfo.ContainsKey("Update"))
            {
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Update"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
    DateTimeKind.Unspecified);
                Update = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
            }

            if (objectInfo.ContainsKey("Options"))
            {
#pragma warning disable 618
               Options = objectInfo["Options"] as string;
#pragma warning restore 618
            }
            else
                Locked = false;

            if (objectInfo.ContainsKey("Description"))
                Description = objectInfo["Description"];

            if (objectInfo.ContainsKey("Revision"))
                Revision = objectInfo["Revision"];

            if (objectInfo.ContainsKey("ServerID"))
                ServerId = objectInfo["ServerID"];
        }
    }
}
