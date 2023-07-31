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
 * Name		: FormSpec.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a form specification in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Field Data Type for a field in a form specification.
	/// </summary>
	[Flags]
	public enum SpecFieldDataType
	{
        /// <summary>
        /// No value.
        /// </summary>
        None = 0x000,
		/// <summary>
		/// Word: a single word (any value).
		/// </summary>
		Word = 0x001,
		/// <summary>
		///  Date: a date/time field.
		/// </summary>
		Date = 0x002,
		/// <summary>
		/// Select: one of a set of words.
		/// </summary>
		Select = 0x004,
		/// <summary>
		/// Line: a one-liner.
		/// </summary>
		Line = 0x008,
        /// <summary>
		/// Text: a block of text.
		/// </summary>
		Text = 0x010,
        /// <summary>
		/// Bulk: text not indexed for 'p4 jobs -e'
		/// </summary>
		Bulk = 0x020
	}

    /// <summary>
	/// Field Type for a field in a form specification.
	/// </summary>
	[Flags]
	public enum SpecFieldFieldType
	{
		/// <summary>
		/// The unique identifier for the field.
		/// </summary>
		Key = 0x000,
		/// <summary>
		///  Required: default provided, value must be present.
		/// </summary>
		Required = 0x001,
		/// <summary>
		/// Always: always set to the default when saving the form.
		/// </summary>
		Always = 0x002,
		/// <summary>
		/// Optional: no default, and not required to be present.
		/// </summary>
		Optional = 0x004,
        /// <summary>
        /// Default: default provided, still not required.
        /// </summary>
        Default =0x008,
        /// <summary>
        /// Once: set once to the default and never changed.
        /// </summary>
        Once = 0x010
	}

	/// <summary>
	/// Specifies structural and semantic metadata for form types.
	/// </summary>
	public class FormSpec
	{
		/// <summary>
		/// Create an empty FormSpec
		/// </summary>
		public FormSpec()
		{
            Fields = new List<SpecField>();
            FieldMap = new Dictionary<string,string>();
			Words = new List<string>();
			Formats = new List<string>();
			Values = new Dictionary<string,string>();
			Presets = new Dictionary<string,string>();
			Openable = new List<string>();
			Maxwords = new List<string>();
			Comments = null;
		}

		/// <summary>
		/// Create a FormSpec
		/// </summary>
        public FormSpec(List<SpecField> fields, Dictionary<String, String> fieldmap, 
            List<String> words, List<String> formats, Dictionary<String, String> values,
			Dictionary<String, String> presets, List<string> openable, List<string> maxwords, string comments)
		{
			Fields = fields;
		    FieldMap = fieldmap;
			Words = words;
			Formats = formats;
			Values = values;
			Presets = presets;
			Openable = openable;
			Maxwords = maxwords;
			Comments = comments;
		}

		/// <summary>
		/// Create a FormSpec
		/// </summary>
		[ObsoleteAttribute("Use FormSpec(List<SpecField> fields, Dictionary<String, String> fieldmap, List < String > words, List < String > formats, Dictionary < String, String > values, Dictionary < String, String > presets, List<string> openable, List<string> maxwords, string comments)", false)]
		public FormSpec(List<SpecField> fields, Dictionary<String, String> fieldmap,
			List<String> words, List<String> formats, Dictionary<String, String> values,
			Dictionary<String, String> presets, string comments)
			: this(fields, fieldmap, words, formats, values, presets, null, null, comments)
		{ 
		}

		/// <summary>
		/// List of all SpecField objects for all fields defined for this Form type. 
		/// </summary>
		public IList<SpecField> Fields { get; set; }
        /// <summary>
        /// Map, keyed by SpecField name, containing suitable allowed values for specific form fields.
        /// </summary>
        public Dictionary<String, String> FieldMap { get; set; }
		/// <summary>
		/// List of "words" for this form type.
		/// </summary>
		public IList<String> Words { get; set; }
		/// <summary>
		/// List of "formats" for this form type.
		/// </summary>
		public IList<String> Formats { get; set; }
		/// <summary>
		/// Map, keyed by SpecField name, containing suitable allowed values for specific form fields.
		/// </summary>
		/// <remarks>
		/// See the main Perforce documentation for formats used here. 
		/// </remarks>
		public Dictionary<String, String> Values { get; set; }
		/// <summary>
		/// Map, keyed by SpecField name, containing preset (default) values for specific form fields.
		/// </summary>
		/// <remarks>
		/// See the main Perforce documentation for formats used here. 
		/// </remarks>
		public Dictionary<String, String> Presets { get; set; }

		public IList<String> Openable { get; set; }
		public IList<String> Maxwords { get; set; }

		/// <summary>
		/// a single (possibly rather long) string (which may contain embedded 
		/// newlines) containing comments to be optionally used in GUI or 
		/// other representations of the form type. 
		/// </summary>
		public string Comments { get; set; }

        /// <summary>
        /// Return the SpecFieldDataType associated with a key in a formspec
        /// </summary>
        /// <param name="formspec">FormSpec to search</param>
        /// <param name="key">Key to query</param>
        /// <returns>The SpecFieldDataType enum</returns>
        public SpecFieldDataType GetSpecFieldDataType(FormSpec formspec, string key)
        {
            string line;
            formspec.FieldMap.TryGetValue(key, out line);

            if (line==null)
            {
                return SpecFieldDataType.None;
            }
            string[] parts = line.Split(new char[] { ' ' }, 4);

            StringEnum<SpecFieldDataType> data = parts[2];
            return data;
        }

		/// <summary>
		/// Create a FormSpec from the tagged output of the 'spec' command
		/// </summary>
		/// <param name="obj">Tagged object returned by the 'spec' command</param>
		/// <returns></returns>
		public static FormSpec FromSpecCmdTaggedOutput(TaggedObject obj)
		{
			FormSpec val = new FormSpec();
			int idx = 0;

			// Check for each property in the tagged data, and use it to set the 
			// appropriate filed in the object

            string key = String.Format("Fields{0}", idx);
			while (obj.ContainsKey(key))
			{
				string fieldDef = obj[key];
				val.Fields.Add(SpecField.FromSpecCmdTaggedData(fieldDef));
				key = String.Format("Fields{0}", ++idx);
			}

            idx = 0;
            key = String.Format("Fields{0}", idx);
            while (obj.ContainsKey(key))
            {
                string line = obj[key];
                string[] parts = line.Split(new char[] { ' ' }, 3);
                val.FieldMap[parts[1]] = line;
                key = String.Format("Fields{0}", ++idx);
            }

			idx = 0;
			key = String.Format("Words{0}", idx);
			while (obj.ContainsKey(key))
			{
				string word = obj[key];
				val.Words.Add(word);
				key = String.Format("Words{0}", ++idx);
			}

			idx = 0;
			key = String.Format("Formats{0}", idx);
			while (obj.ContainsKey(key))
			{
				string word = obj[key];
				val.Formats.Add(word);
				key = String.Format("Formats{0}", ++idx);
			}

			idx = 0;
			key = String.Format("Values{0}", idx);
			while (obj.ContainsKey(key))
			{
				string line = obj[key];
				string[] parts = line.Split(new char[] { ' ' }, 2);
				val.Values[parts[0]] = parts[1];
				key = String.Format("Values{0}", ++idx);
			}

			idx = 0;
			key = String.Format("Presets{0}", idx);
			while (obj.ContainsKey(key))
			{
				string line = obj[key];
				string[] parts = line.Split(new char[] { ' ' }, 2);
				val.Presets[parts[0]] = parts[1];
				key = String.Format("Presets{0}", ++idx);
			}

			idx = 0;
			key = String.Format("Openable{0}", idx);
			while (obj.ContainsKey(key))
			{
				string word = obj[key];
				val.Openable.Add(word);
				key = String.Format("Openable{0}", ++idx);
			}

			idx = 0;
			key = String.Format("Maxwords{0}", idx);
			while (obj.ContainsKey(key))
			{
				string word = obj[key];
				val.Maxwords.Add(word);
				key = String.Format("Maxwords{0}", ++idx);
			}

			if (obj.ContainsKey("Comments"))
			{
				val.Comments = obj["Comments"];
			}

			return val;
		}
	}

	/// <summary>
	/// Class representing a field in a FormSpec.
	/// </summary>
	public class SpecField
	{
		/// <summary>
		/// Create a default FormSpec
		/// </summary>
		public SpecField()
		{
			Code = -1;
			Name = null;
			DataType = SpecFieldDataType.Word; ;
			Length = 0;
			FieldType = SpecFieldFieldType.Optional;
		}
		/// <summary>
		/// Create a FormSpec
		/// </summary>
		public SpecField(int code, string name, SpecFieldDataType datatype, int length, SpecFieldFieldType fieldtype)
		{
			Code = code;
			Name = name;
			DataType = datatype;
			Length = length;
			FieldType = fieldtype;
		}

		/// <summary>
		/// Numeric code identifying this form field. 
		/// </summary>
		public int Code { get; set;}
		/// <summary>
		/// Name of this form field. 
		/// </summary>
		public string Name { get; set; }

		private StringEnum<SpecFieldDataType> _dataType;
		/// <summary>
		/// A field's type, i.e. whether it's a single word, a date, a selection, or a text field
		/// </summary>
		public SpecFieldDataType DataType 
		{
			get { return _dataType; }
			set { _dataType = value; } 
		}
		/// <summary>
		/// The maximum length in characters (?) of this field. 
		/// </summary>
		int Length { get; set; }

		private StringEnum<SpecFieldFieldType> _fieldType;
		/// <summary>
		/// Specifies whether the field is optional, required, a key, or set by the server. 
		/// </summary>
		public SpecFieldFieldType FieldType 
		{
			get { return _fieldType; }
			set { _fieldType = value; } 
		}

		/// <summary>
		/// Create a SpecField from a 'Fields' entry in the tagged data from the 'spec' command.
		/// </summary>
		/// <param name="def"></param>
		/// <returns></returns>
		public static SpecField FromSpecCmdTaggedData(string def)
		{
			SpecField val = new SpecField();

			string[] parts = def.Split(' ');

			int v =-1;
			int.TryParse(parts[0], out v);
			val.Code = v;

			val.Name = parts[1];

			val._dataType = parts[2];

			v =-1;
			int.TryParse(parts[3], out v);
			val.Length = v;

			val._fieldType = parts[4];

			return val;
		}
	}

}
