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
 * Name		: Stream.cs
 *
 * Author	: wjb
 *
 * Description	: Class used to abstract a stream in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
    /// <summary>
    /// A stream specification in a Perforce repository. 
    /// </summary>
    public class Stream
    {
        public Stream() { _type = "development"; }

        public Stream(
            string id,
            DateTime updated,
            DateTime accessed,
            string ownername,
            string name,
            PathSpec parent,
            PathSpec baseparent,
            StreamType type,
            string description,
            StreamOption options,
            string firmerthanparent,
            string changeflowstoparent,
            string changeflowsfromparent,
            ViewMap paths,
            ViewMap remapped,
            ViewMap ignored,
            ViewMap view,
            ViewMap changeview,
            FormSpec spec,
            Dictionary<string, object> customfields,
            ParentView parentview)

        {
            Id = id;
            Updated = updated;
            Accessed = accessed;
            OwnerName = ownername;
            Name = name;
            Parent = parent;
            BaseParent = baseparent;
            Type = type;
            Description = description;
            Options = options;
            FirmerThanParent = firmerthanparent;
            ChangeFlowsToParent = changeflowstoparent;
            ChangeFlowsFromParent = changeflowsfromparent;
            Paths = paths;
            Remapped = remapped;
            Ignored = ignored;
            View = view;
            ChangeView = changeview;
            Spec = spec;
            CustomFields = customfields;
            ParentView = parentview;
        }


        /// <summary>
        /// A stream specification in a Perforce repository. 
        /// </summary>
        [ObsoleteAttribute("Use Stream(string id, DateTime updated, DateTime accessed, string ownername, string name, PathSpec parent, PathSpec baseparent, StreamType type, string description, StreamOption options, string firmerthanparent, string changeflowstoparent, string changeflowsfromparent, ViewMap paths, ViewMap remapped, ViewMap ignored, ViewMap view, ViewMap changeview, FormSpec spec, Dictionary<string, object> customfields)", false)]
        public Stream(
            string id,
            DateTime updated,
            DateTime accessed,
            string ownername,
            string name,
            PathSpec parent,
            PathSpec baseparent,
            StreamType type,
            string description,
            StreamOption options,
            string firmerthanparent,
            string changeflowstoparent,
            string changeflowsfromparent,
            ViewMap paths,
            ViewMap remapped,
            ViewMap ignored,
            ViewMap view,
            FormSpec spec)
        { 
            Id = id;
            Updated = updated;
            Accessed = accessed;
            OwnerName = ownername;
            Name = name;
            Parent = parent;
            BaseParent = baseparent;
            Type = type;
            Description = description;
            Options = options;
            FirmerThanParent = firmerthanparent;
            ChangeFlowsToParent = changeflowstoparent;
            ChangeFlowsFromParent = changeflowsfromparent;
            Paths = paths;
            Remapped = remapped;
            Ignored = ignored;
            View = view;
            Spec = spec;
        }

        private Dictionary<string, string> specDef = new Dictionary<string, string>();

        private FormBase _baseForm;

        #region properties

        public string Id { get; set; }
        public DateTime Updated { get; set; }
        public DateTime Accessed { get; set; }
        public string OwnerName { get; set; }
        public string Name { get; set; }
        public PathSpec Parent { get; set; }
        public PathSpec BaseParent { get; set; }

        private StringEnum<StreamType> _type;
        public StreamType Type
        {
            get { return _type; }
            set { _type = value; }
        }

        public string Description { get; set; }
        public string FirmerThanParent { get; set; }
        public string ChangeFlowsToParent { get; set; }
        public string ChangeFlowsFromParent { get; set; }

        private StreamOptionEnum _options;
        public StreamOption Options
        {
            get { return _options; }
            set { _options = (StreamOptionEnum)value; }
        }

        public ViewMap Paths { get; set; }
        public ViewMap Remapped { get; set; }
        public ViewMap Ignored { get; set; }
        public ViewMap View { get; private set; }
        public FormSpec Spec { get; set; }
        public Dictionary<string, object> CustomFields { get; set; } = new Dictionary<string, object>();
        public ViewMap ChangeView { get; set; }


        private StringEnum<ParentView> _parentView;
        public ParentView ParentView
        {
            get { return _parentView; }
            set { _parentView = value; }
        }

	#endregion

	private void PopulateCommonFields(TaggedObject objectInfo)
        {

            if (objectInfo.ContainsKey("Stream"))
            {
                Id = objectInfo["Stream"];
                specDef.Remove("Stream");
            }

            if (objectInfo.ContainsKey("Owner"))
            {
                OwnerName = objectInfo["Owner"];
                specDef.Remove("Owner");
            }

            if (objectInfo.ContainsKey("Name"))
            {
                Name = objectInfo["Name"];
                specDef.Remove("Name");
            }

            if (objectInfo.ContainsKey("Parent"))
            {
                Parent = new DepotPath(objectInfo["Parent"]);
                specDef.Remove("Parent");
            }

            if (objectInfo.ContainsKey("baseParent"))
            {
                BaseParent = new DepotPath(objectInfo["baseParent"]);
                specDef.Remove("baseParent");
            }

            if (objectInfo.ContainsKey("Type"))
            {
                _type = (objectInfo["Type"]);
                specDef.Remove("Type");
            }

            if (objectInfo.ContainsKey("Description"))
            {
                Description = objectInfo["Description"];
                specDef.Remove("Description");
            }

            if (objectInfo.ContainsKey("desc"))
            {
                Description = objectInfo["desc"];
                specDef.Remove("desc");
            }

            if (objectInfo.ContainsKey("Options"))
            {
                String optionsStr = objectInfo["Options"];
                _options = optionsStr;
                specDef.Remove("Options");
            }

            if (objectInfo.ContainsKey("firmerThanParent"))
            {
                FirmerThanParent = objectInfo["firmerThanParent"];
                specDef.Remove("firmerThanParent");
            }

            if (objectInfo.ContainsKey("changeFlowsToParent"))
            {
                ChangeFlowsToParent = objectInfo["changeFlowsToParent"];
                specDef.Remove("changeFlowsToParent");
            }

            if (objectInfo.ContainsKey("changeFlowsFromParent"))
            {
                ChangeFlowsFromParent = objectInfo["changeFlowsFromParent"];
                specDef.Remove("changeFlowsFromParent");
            }

            if (objectInfo.ContainsKey("ParentView"))
            {
                _parentView = (objectInfo["ParentView"]);
                specDef.Remove("ParentView");
            }
        }

        /// <summary>
        /// Read the fields from the tagged output of a 'streams' command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'streams' command</param>
        /// <param name="offset">Date processing</param>
        /// <param name="dst_mismatch">DST for date</param>
        public void FromStreamsCmdTaggedOutput(TaggedObject objectInfo, string offset, bool dst_mismatch)
        {
            _baseForm = new FormBase();

            _baseForm.SetValues(objectInfo);

            GetSpecDefinition(objectInfo);

            if (objectInfo.ContainsKey("Update"))
            {
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Update"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);
                Updated = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
                specDef.Remove("Update");
            }

            if (objectInfo.ContainsKey("Access"))
            {
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Access"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);
                Accessed = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
                specDef.Remove("Access");
            }

            PopulateCommonFields(objectInfo);

            SetCustomFields();
        }

        /// <summary>
        /// Read the fields from the tagged output of a 'stream' command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'stream' command</param>
        public void FromStreamCmdTaggedOutput(TaggedObject objectInfo)
        {
            _baseForm = new FormBase();

            _baseForm.SetValues(objectInfo);

            GetSpecDefinition(objectInfo);

            if (objectInfo.ContainsKey("Update"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(objectInfo["Update"], out v);
                Updated = v;
                specDef.Remove("Update");
            }

            if (objectInfo.ContainsKey("Access"))
            {
                DateTime v = DateTime.MinValue;
                DateTime.TryParse(objectInfo["Access"], out v);
                Accessed = v;
                specDef.Remove("Access");
            }

            Paths               = ReadViewField(objectInfo, "Paths");
            Remapped            = ReadViewField(objectInfo, "Remapped");
            Ignored             = ReadViewField(objectInfo, "Ignored");
            View                = ReadViewField(objectInfo, "View");
            ChangeView          = ReadViewField(objectInfo, "ChangeView");

            PopulateCommonFields(objectInfo);

            SetCustomFields();
        }


        private ViewMap ReadViewField(TaggedObject objectInfo, String field)
        {
            if (specDef.ContainsKey(field))
            {
                specDef.Remove(field);
            }
           

            ViewMap map = new ViewMap();
            int idx = 0;
            int mapIdx = 0;
            string key = String.Format("{0}{1}", field, idx);
            string commentKey = String.Format("{0}Comment{1}", field, idx);
            
            if (!objectInfo.ContainsKey(key))
                return null;

            while (objectInfo.ContainsKey(key))
            {
                bool haveComment = objectInfo.ContainsKey(commentKey);
                if ((objectInfo[key] != "") || (haveComment))
                {
                    map.Add(objectInfo[key]);
                    if (haveComment)
                    {
                        map[mapIdx].Comment = objectInfo[commentKey].TrimEnd();
                    }
                    mapIdx++;
                }
                idx++;
                key = String.Format("{0}{1}", field, idx);
                commentKey = String.Format("{0}Comment{1}", field, idx);
            }
            return map;
        }


        /// <summary>
        /// Parse the fields from a stream specification 
        /// </summary>
        /// <param name="spec">Text of the stream specification in server format</param>
        /// <returns></returns>
        public bool Parse(String spec)
        {
            _baseForm = new FormBase();
            _baseForm.Parse(spec); // parse the values into the underlying dictionary
            GetSpecDefinition(_baseForm);

            Id                  = ParseSingleString(_baseForm, "Stream");
            OwnerName           = ParseSingleString(_baseForm, "Owner");
            Name                = ParseSingleString(_baseForm, "Name");            
            Description         = ParseDescription(_baseForm);
            Ignored             = ParseView(_baseForm, "Ignored");
            Paths               = ParseView(_baseForm, "Paths");
            Remapped            = ParseView(_baseForm, "Remapped");
            View                = ParseView(_baseForm, "View");
            ChangeView          = ParseView(_baseForm, "ChangeView");
            Updated             = ParseDate(_baseForm, "Update");
            Accessed            = ParseDate(_baseForm, "Access");

            if (_baseForm.ContainsKey("Options"))
            {
                _options = _baseForm["Options"] as string;
                specDef.Remove("Options");
            }

            if (_baseForm.ContainsKey("Type"))
            {
                _type = _baseForm["Type"] as string;
                specDef.Remove("Type");
            }

            if (_baseForm.ContainsKey("ParentView"))
            {
                _parentView = (_baseForm["ParentView"].ToString() == "inherit") ? ParentView.Inherit : ParentView.NoInherit;
                //_parentView = _baseForm["ParentView"] as string;
                specDef.Remove("ParentView");
            }

            if (_baseForm.ContainsKey("Parent"))
            {
                string line = _baseForm["Parent"] as string;
                Parent = new DepotPath(line);
                specDef.Remove("Parent");
            }

            SetCustomFields();
    
                return true;
        }

        private string ParseSingleString(FormBase _baseForm, string field)
        {
            string result = null;
            if (_baseForm.ContainsKey(field))
            {
                result = _baseForm[field] as string;
                specDef.Remove(field);
            }
            return result;
        }

        private ViewMap ParseView(FormBase _baseForm, string field)
        {
            ViewMap view = new ViewMap();

            if (_baseForm.ContainsKey(field))
            {
                SimpleList<string> _list = (SimpleList<string>)_baseForm[field];
                string[] list = _list.ToArray();
                foreach (string line in list)
                {
                    view.Add(line);
                }
            }
            else
            {
                view = null;
            }

            if (specDef.ContainsKey(field))
            {
                specDef.Remove(field);
            }
            return view;
        }

        private string ParseDescription(FormBase _baseForm)
        {
            string result = string.Empty;
            string key = "Description";
            if (_baseForm.ContainsKey(key))
            {
                SimpleList<string> _list = (SimpleList<string>)_baseForm[key];
                string[] list = _list.ToArray();
                foreach (string line in list)
                {
                    result += line + "\r\n\t";
                }
                result = result.Substring(0, result.Length - 3);
            }

            if (specDef.ContainsKey(key))
            {
                specDef.Remove(key);
            }

            return result;
        }

        private DateTime ParseDate(FormBase _baseForm, string field)
        {
            DateTime d = new DateTime();

            if (DateTime.TryParse(_baseForm[field] as string, out d))
            {
                specDef.Remove(field);
            }

            return d;
        }

        /// <summary>
        /// Format of a stream specification used to save a stream to the server
        /// </summary>
        private static String StreamSpecFormat =
                                                    "Stream:\t{0}\r\n" +
                                                    "\r\n" +
                                                    "Update:\t{1}\r\n" +
                                                    "\r\n" +
                                                    "Access:\t{2}\r\n" +
                                                    "\r\n" +
                                                    "Owner:\t{3}\r\n" +
                                                    "\r\n" +
                                                    "Name:\t{4}\r\n" +
                                                    "\r\n" +
                                                    "Parent:\t{5}\r\n" +
                                                    "\r\n" +
                                                    "Type:\t{6}\r\n" +
                                                    "\r\n" +
                                                    "Description:\r\n\t{7}\r\n" +
                                                    "\r\n" +
                                                    "Options:\t{8}\r\n" +
                                                    "\r\n" +
                                                    "ParentView:\t{9}\r\n" +
                                                    "\r\n" +
                                                    "Paths:\r\n\t{10}\r\n" +
                                                    "\r\n" +
                                                    "Remapped:\r\n\t{11}\r\n" +
                                                    "\r\n" +
                                                    "Ignored:\r\n\t{12}\r\n" +
                                                    "\r\n" +
                                                    "{13}";

        /// <summary>
        /// Convert to specification in server format
        /// </summary>
        /// <returns></returns>
        override public String ToString()
        {
            String descStr = String.Empty;
            if (Description != null)
                descStr = Description.Replace("\n", "\n\t");

            String Type = _type.ToString(StringEnumCase.Lower);

            String ParentPath = String.Empty;
            if (Parent != null)
            {
                ParentPath = Parent.Path.ToString();
                if (ParentPath == "None")
                {
                    ParentPath = ParentPath.ToLower();
                }
            }

            String pathsView = String.Empty;
            if (Paths != null)
                pathsView = Paths.ToString().Replace("\r\n", "\r\n\t").Trim();

            String remappedView = String.Empty;
            if (Remapped != null)
                remappedView = Remapped.ToString().Replace("\r\n", "\r\n\t").Trim();

            String ignoredView = String.Empty;
            if (Ignored != null)
                ignoredView = Ignored.ToString().Replace("\r\n", "\r\n\t").Trim();

            string customFieldsString = String.Empty;
            if (CustomFields.Count != 0)
                customFieldsString = CustomFieldsToString();

            String ParentView = _parentView.ToString(StringEnumCase.Lower);

            String value = String.Format(StreamSpecFormat, Id,
                FormBase.FormatDateTime(Updated), FormBase.FormatDateTime(Accessed),
                OwnerName, Name, ParentPath, Type, descStr, _options.ToString(),
                ParentView, pathsView, remappedView, ignoredView, customFieldsString);
            return value;
        }

        private string CustomFieldsToString()
        {
            string result = string.Empty;
            foreach (KeyValuePair<string, object> kvp in CustomFields)
            {
                if (kvp.Value is int)
                {
                    result += kvp.Key.ToString() + ":\t" + kvp.Value.ToString() + "\r\n\r\n";
                }

                if (kvp.Value is String)
                {
                    result += kvp.Key.ToString() + ":\t" + kvp.Value.ToString() + "\r\n\r\n";
                }

                if (kvp.Value is List<string>)
                {
                    result += kvp.Key.ToString() + ":\r\n";
                    foreach (string foo in (List<string>)kvp.Value)
                    {
                        result += "\t" + foo + "\r\n";
                    }
                    //result = result.Substring(0, result.Length - 3);
                    result += "\r\n";
                }
            }
            return result.Substring(0, result.Length - 2);
        }
        
        private void GetSpecDefinition(FormBase _baseForm)
        {
            if (_baseForm.ContainsKey("specdef"))
            {
                string tmp = _baseForm["specdef"] as string;
                string[] fields = tmp.Split(new string[] { ";;" }, StringSplitOptions.None);

                foreach (string field in fields)
                {
                    string value = "word";
                    string[] line = field.Split(';');
                    foreach (string x in line)
                    {
                        if (x.Contains("type"))
                        {
                            string[] typeTmp = x.Split(':');
                            value = typeTmp[1];
                        }
                    }
                    specDef.Add(line[0], value);
                }
            }
        }

        private void SetCustomFields()
        {
            foreach (KeyValuePair<string, string> kvp in specDef)
            {
                if (_baseForm.ContainsKey(kvp.Key))
                {
                    CustomFields.Add(kvp.Key, _baseForm[kvp.Key]);
                }
            }
        }

        private void GetSpecDefinition(TaggedObject objectInfo)
        {
            _baseForm = new FormBase();
            _baseForm.SetValues(objectInfo);
            GetSpecDefinition(_baseForm);
        }
    }

    /// <summary>
    /// Defines the expected flow of change between a stream and its parent.	
    /// </summary>
    [Flags]
    public enum StreamType
    {
        /// <summary>
        /// Development: Default. Direction of flow is
        /// to parent stream with copy and from parent
        /// stream with merge.
        /// </summary>
        Development = 0x0000,
        /// <summary>
        /// Mainline: May not have a parent. 
        /// </summary>
        Mainline = 0x0001,
        /// <summary>
        /// Release: Direction of flow is to parent
        /// with merge and from parent with copy.
        /// </summary>
        Release = 0x0002,
        /// <summary>
        /// Virtual: Not a stream but an alternative 
        /// view of its parent stream.
        /// </summary>
        Virtual = 0x0004,
        /// <summary>
        /// Task: A lightweight short-lived stream that
        ///  only promotes modified content to the
        ///  repository, branched data is stored in
        ///  shadow tables that are removed when the task
        ///  stream is deleted or unloaded.
        /// </summary>
        Task = 0x0008
    }

    /// <summary>
    /// Flags to configure stream behavior.
    /// </summary>
    [Flags]
    public enum StreamOption
    {
        /// <summary>
        /// No flags.
        /// </summary>
        None = 0x0000,
        /// <summary>
        /// Indicates whether all users or only the
        /// of the stream may submit changes to the
        /// stream path.
        /// </summary>
        OwnerSubmit = 0x0001,
        /// <summary>
        /// Indicates whether the stream spec is locked
        /// against modifications. If locked, the spec
        /// may not be deleted, and only its owner may
        /// modify it.
        /// </summary>
        Locked = 0x0002,
        /// <summary>
        /// Indicates whether integration from the
        /// stream to its parent is expected to occur.
        /// </summary>
        NoToParent = 0x0004,
        /// <summary>
        /// Indicates whether integration to the stream
        /// from its parent is expected to occur.
        /// </summary>
        NoFromParent = 0x0008,
        /// <summary>
        /// Indicates whether integration to the stream
        /// from its parent is expected to occur.
        /// </summary>
        MergeAny = 0x0010
    }

    /// <summary>
    /// Flags to configure stream ParentView.
    /// </summary>
    [Flags]
    public enum ParentView
    {
        /// <summary>
        /// Unset read default from server.
        /// </summary>
        None = 0x0000,
        /// <summary>
        /// If a ParentView is inherit, the Paths, Remapped, and Ignored
        /// fields will be affected.  The view created from each field is
        /// composed of the stream's fields and the set of fields
        /// "inherited" from each of the stream's ancestors. The
        /// inheritance is implicit, so the inherited Paths, Remapped,
        /// and Ignored values will not be displayed with the current
        /// stream specification.
        /// </summary>
        Inherit = 0x0001,
        /// <summary>
        /// If a ParentView is noinherit, the Paths, Remapped, and
        /// Ignored fields are not affected by the stream's ancestors.
        /// The child views are exactly what is specified in the Paths,
        /// Remapped, and Ignored fields.
        /// </summary>
        NoInherit = 0x0002,        
    }

    internal class StreamOptionEnum : StringEnum<StreamOption>
    {

        public StreamOptionEnum(StreamOption v)
            : base(v)
        {
        }

        public StreamOptionEnum(string spec)
            : base(StreamOption.None)
        {
            Parse(spec);
        }

        public static implicit operator StreamOptionEnum(StreamOption v)
        {
            return new StreamOptionEnum(v);
        }

        public static implicit operator StreamOptionEnum(string s)
        {
            return new StreamOptionEnum(s);
        }

        public static implicit operator string(StreamOptionEnum v)
        {
            return v.ToString();
        }

        public override bool Equals(object obj)
        {
            if (obj.GetType() == typeof(StreamOption))
            {
                return value.Equals((StreamOption)obj);
            }
            if (obj.GetType() == typeof(StreamOptionEnum))
            {
                return value.Equals(((StreamOptionEnum)obj).value);
            }
            return false;
        }
        public override int GetHashCode() { return base.GetHashCode(); }

        public static bool operator ==(StreamOptionEnum t1, StreamOptionEnum t2) { return t1.value.Equals(t2.value); }
        public static bool operator !=(StreamOptionEnum t1, StreamOptionEnum t2) { return !t1.value.Equals(t2.value); }

        public static bool operator ==(StreamOption t1, StreamOptionEnum t2) { return t1.Equals(t2.value); }
        public static bool operator !=(StreamOption t1, StreamOptionEnum t2) { return !t1.Equals(t2.value); }

        public static bool operator ==(StreamOptionEnum t1, StreamOption t2) { return t1.value.Equals(t2); }
        public static bool operator !=(StreamOptionEnum t1, StreamOption t2) { return !t1.value.Equals(t2); }

        /// <summary>
        /// Convert to a stream spec formatted string
        /// </summary>
        /// <returns></returns>
        public override string ToString()
        {
            return String.Format("{0} {1} {2} {3} {4}",
                ((value & StreamOption.OwnerSubmit) != 0) ? "ownersubmit" : "allsubmit",
                ((value & StreamOption.Locked) != 0) ? "locked" : "unlocked",
                ((value & StreamOption.NoToParent) != 0) ? "notoparent" : "toparent",
                ((value & StreamOption.NoFromParent) != 0) ? "nofromparent" : "fromparent",
                ((value & StreamOption.MergeAny) != 0) ? "mergeany" : "mergedown"
                );
        }

        /// <summary>
        /// Parse a client spec formatted string
        /// </summary>
        /// <param name="spec"></param>
        public void Parse(String spec)
        {
            value = StreamOption.None;

            if (!spec.Contains("allsubmit"))
                value |= StreamOption.OwnerSubmit;

            if (!spec.Contains("unlocked"))
                value |= StreamOption.Locked;

            if (spec.Contains("notoparent"))
                value |= StreamOption.NoToParent;

            if (spec.Contains("nofromparent"))
                value |= StreamOption.NoFromParent;

            if (spec.Contains("mergeany"))
                value |= StreamOption.MergeAny;
        }
    }
}
