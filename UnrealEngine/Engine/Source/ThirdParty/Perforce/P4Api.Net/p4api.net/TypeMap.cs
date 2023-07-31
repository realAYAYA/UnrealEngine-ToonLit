using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Defines a Perforce repository's default mapping between
	/// file names or locations and file types. 
	/// </summary>
	public class TypeMap : List<TypeMapEntry>
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		public TypeMap() { }

        /// <summary>
        /// Parameterized Constructor - Creates typemap with one entry
        /// </summary>
        /// <param name="mapping">entry to store</param>
        /// <param name="spec">FormSpec to store</param>
		public TypeMap
			(
			TypeMapEntry mapping,
			FormSpec spec
			)
		{
			Mapping = mapping;
			Spec = spec;
		}


		public TypeMapEntry Mapping { get; set; }
		public FormSpec Spec { get; set; }
	}

	/// <summary>
	/// Describes an individual entry in the Perforce repository's typemap.
	/// </summary>
	public class TypeMapEntry
	{
        /// <summary>
        /// Constructor for a line in the typemap
        /// </summary>
        /// <param name="filetype">file type of files specified by path</param>
        /// <param name="path">path to files which are of type filetype</param>
		public TypeMapEntry 
			(
			FileType filetype,
			string path
			)
		{
			FileType = filetype;
			Path = path;
		}

        /// <summary>
        /// Construct a line in the typemap from a string
        /// </summary>
        /// <param name="spec">string to parse</param>
		public TypeMapEntry (string spec)
		{
			Parse(spec);
		}

        /// <summary>
        /// Property to access FileType of this entry
        /// </summary>
		public FileType FileType { get; set; }

        /// <summary>
        /// Property to access Path of this entry
        /// </summary>
		public string Path { get; set; }

        /// <summary>
        /// Decode a serialized typemap entry 
        /// </summary>
        /// <param name="spec">string containing typemap entry</param>
		public void Parse(string spec)
		{
			int idx = spec.IndexOf(' ');
			string ftstr = spec.Substring(0, idx);
			this.FileType = new FileType(ftstr);
			this.Path = spec.Substring(idx + 1);
		}

        /// <summary>
        /// Return a string describing a typemap entry
        /// </summary>
        /// <returns>string description</returns>
		public override string ToString()
		{
			return String.Format("{0} {1}", this.FileType.ToString(), this.Path);
		}
	}

		
}
