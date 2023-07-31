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
 * Name		: BranchSpec.cs
 *
 * Author	: wjb
 *
 * Description	: Class used to abstract a branch specification in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
		/// <remarks>
		/// <br/><b>p4 help branch</b>
		/// <br/> 
		/// <br/>     branch -- Create, modify, or delete a branch view specification
		/// <br/> 
		/// <br/>     p4 branch [-f] name
		/// <br/>     p4 branch -d [-f] name
		/// <br/>     p4 branch [ -S stream ] [ -P parent ] -o name
		/// <br/>     p4 branch -i [-f]
		/// <br/> 
		/// <br/> 	A branch specification ('spec') is a named, user-defined mapping of
		/// <br/> 	depot files to depot files. It can be used with most of the commands
		/// <br/> 	that operate on two sets of files ('copy', 'merge', 'integrate',
		/// <br/> 	'diff2', etc.)
		/// <br/> 
		/// <br/> 	Creating a branch spec does not branch files.  To branch files, use
		/// <br/> 	'p4 copy', with or without a branch spec.
		/// <br/> 	
		/// <br/> 	The 'branch' command puts the branch spec into a temporary file and
		/// <br/> 	invokes the editor configured by the environment variable $P4EDITOR.
		/// <br/> 	Saving the file creates or modifies the branch spec.
		/// <br/> 
		/// <br/> 	The branch spec contains the following fields:
		/// <br/> 
		/// <br/> 	Branch:      The branch spec name (read only).
		/// <br/> 
		/// <br/> 	Owner:       The user who created this branch spec. Can be changed.
		/// <br/> 
		/// <br/> 	Update:      The date this branch spec was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date of the last command used with this spec.
		/// <br/> 
		/// <br/> 	Description: A description of the branch spec (optional).
		/// <br/> 
		/// <br/> 	Options:     Flags to change the branch spec behavior. The defaults
		/// <br/> 		     are marked with *.
		/// <br/> 
		/// <br/> 		locked   	Permits only the owner to change the spec.
		/// <br/> 		unlocked *	Prevents the branch spec from being deleted.
		/// <br/> 
		/// <br/> 	View:        Lines mapping of one view of depot files to another.
		/// <br/> 		     Both the left and right-hand sides of the mappings refer
		/// <br/> 		     to the depot namespace.  See 'p4 help views' for more on
		/// <br/> 		     view syntax.
		/// <br/> 
		/// <br/> 	New branch specs are created with a default view that maps all depot
		/// <br/> 	files to themselves.  This view must be changed before the branch
		/// <br/> 	spec can be saved.
		/// <br/> 
		/// <br/> 	The -d flag deletes the named branch spec.
		/// <br/> 
		/// <br/> 	The -o flag writes the branch spec to standard output. The user's
		/// <br/> 	editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag causes a branch spec to be read from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag enables a user with 'admin' privilege to delete the spec
		/// <br/> 	or set the 'last modified' date.  By default, specs can be deleted
		/// <br/> 	only by their owner.
		/// <br/> 
		/// <br/> 	A branch spec can also be used to expose the internally generated
		/// <br/> 	mapping of a stream to its parent. (See 'p4 help stream' and 'p4
		/// <br/> 	help streamintro'.)
		/// <br/> 
		/// <br/> 	The -S stream flag will expose the internally generated mapping.
		/// <br/> 	The -P flag may be used with -S to treat the stream as if it were a
		/// <br/> 	child of a different parent. The -o flag is required with -S.
		/// <br/> 
		/// <br/> 
		/// </remarks>
	/// <summary>
	/// A branch view specification in a Perforce repository. 
	/// </summary>
	public class BranchSpec
	{
		/// <summary>
		/// A branch view specification in a Perforce repository. 
		/// </summary>
		public BranchSpec()
		{
		}
		/// <summary>
		/// A branch view specification in a Perforce repository. 
		/// </summary>
		/// <param name="accessed">The date of the last command used with this spec.</param>
		/// <param name="description">A description of the branch spec (optional).</param>
		/// <param name="id">The branch spec name (read only).</param>
		/// <param name="locked">When true, permits only the owner to change the spec.</param>
		/// <param name="options">Flags to change the branch spec behavior.</param>
		/// <param name="owner">The user who created this branch spec.</param>
		/// <param name="spec">Specifies structural and semantic metadata for form types.</param>
		/// <param name="updated">The date this branch spec was last modified.</param>
		/// <param name="viewmap">Lines mapping of one view of depot files to another.</param>
		public BranchSpec(string id,
			string owner,
			DateTime updated,
			DateTime accessed,
			string description,
			bool locked,
			ViewMap viewmap,
			FormSpec spec,
			string options
			)
		{
			Id = id;
			Owner = owner;
			Updated = updated;
			Accessed = accessed;
			Description = description;
			Locked = locked;
			ViewMap = viewmap;
			Spec = spec;
#pragma warning disable 618
         Options = options;
#pragma warning restore 618
      }

		private FormBase _baseForm;

		#region properties
		/// <summary>
		/// The branch spec name (read only).
		/// </summary>
		public string Id { get; set; }
		/// <summary>
		/// The user who created this branch spec. Can be changed.
		/// </summary>
		public string Owner { get; set; }
		/// <summary>
		/// The date this branch spec was last modified.
		/// </summary>
		public 	DateTime Updated { get; set; }
		/// <summary>
		/// The date of the last command used with this spec.
		/// </summary>
		public 	DateTime Accessed { get; set; }
		/// <summary>
		/// A description of the branch spec (optional).
		/// </summary>
		public 	string Description { get; set; }
		/// <summary>
		/// When true, permits only the owner to change the spec.
		/// </summary>
		public 	bool Locked { get; set; }
		/// <summary>
		/// Lines mapping of one view of depot files to another.
		/// </summary>
		public 	ViewMap ViewMap { get; set; }
		/// <summary>
		/// Specifies structural and semantic metadata for form types.
		/// </summary>
		public 	FormSpec Spec { get; set; }
		/// <summary>
		/// Flags to change the branch spec behavior.
		/// </summary>
        [Obsolete("Use Locked Property")]
		public string Options
	    {
	        get
	        {
                return Locked?"locked":string.Empty;
	        }
                 set
                 {
                     Locked = (value == "locked");
                 }
	    }
		#endregion
		#region fromTaggedOutput
		/// <summary>
		/// Read the fields from the tagged output of a branch command
		/// </summary>
		/// <param name="objectInfo">Tagged output from the 'branch' command</param>
        public void FromBranchSpecCmdTaggedOutput(TaggedObject objectInfo)
		{
			_baseForm = new FormBase();

			_baseForm.SetValues(objectInfo);

			if (objectInfo.ContainsKey("Branch"))
				Id = objectInfo["Branch"];

			if (objectInfo.ContainsKey("Owner"))
				Owner = objectInfo["Owner"];

			if (objectInfo.ContainsKey("Update"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(objectInfo["Update"], out v);
				Updated = v;
			}

			if (objectInfo.ContainsKey("Access"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(objectInfo["Access"], out v);
				Accessed = v;
			}

			if (objectInfo.ContainsKey("Description"))
				Description = objectInfo["Description"];

			if (objectInfo.ContainsKey("Options"))
			{
				if(objectInfo["Options"] == "locked")
				Locked = true;
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

		#region client spec support
		/// <summary>
		/// Parse the fields from a branch specification 
		/// </summary>
		/// <param name="spec">Text of the branch specification in server format</param>
		/// <returns></returns>
		public bool Parse(String spec)
		{
			_baseForm = new FormBase();

			_baseForm.Parse(spec); // parse the values into the underlying dictionary

			if (_baseForm.ContainsKey("Branch"))
			{
				Id = _baseForm["Branch"] as string;
			}

			if (_baseForm.ContainsKey("Owner"))
			{
                if (_baseForm["Owner"] is string)
                {
                    Owner = _baseForm["Owner"] as string;
                }
                if (_baseForm["Owner"] is IList<string>)
                {
                    IList<string> strList = _baseForm["Owner"] as IList<string>;
                    Owner = string.Empty;
                    for (int idx = 0; idx < strList.Count; idx++)
                    {
                        if (idx > 0)
                        {
                            Owner += "\r\n";
                        }
                        Owner += strList[idx];
                    }
                }
                if (_baseForm["Owner"] is SimpleList<string>)
                {
                    SimpleList<string> strList = _baseForm["Owner"] as SimpleList<string>;
                    Owner = string.Empty;
                    SimpleListItem<string> current = strList.Head;
                    bool addCRLF = false;
                    while (current != null)
                    {
                        if (addCRLF)
                        {
                            Owner += "\r\n";
                        }
                        else
                        {
                            addCRLF = true;
                        }
                        Owner += current.Item;
                        current = current.Next;
                    }
                }
            }

			if (_baseForm.ContainsKey("Update"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(_baseForm["Update"] as string, out v);
				Updated = v;
			}

			if (_baseForm.ContainsKey("Access"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(_baseForm["Access"] as string, out v);
				Accessed = v;
			}

            if (_baseForm.ContainsKey("Description"))
            {
                if (_baseForm["Description"] is string)
                {
                    Description = _baseForm["Description"] as string;
                }
                if (_baseForm["Description"] is IList<string>)
                {
                    IList<string> strList = _baseForm["Description"] as IList<string>;
                    Description = string.Empty;
                    for (int idx = 0; idx < strList.Count; idx++)
                    {
                        if (idx > 0)
                        {
                            Description += "\r\n";
                        }
                        Description += strList[idx];
                    }
                }
                if (_baseForm["Description"] is SimpleList<string>)
                {
                    SimpleList<string> strList = _baseForm["Description"] as SimpleList<string>;
                    Description = string.Empty;
                    SimpleListItem<string> current = strList.Head;
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

            if (_baseForm.ContainsKey("View"))
            {
                if (_baseForm["View"] is IList<string>)
                {
                    IList<string> lines = _baseForm["View"] as IList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
                else if (_baseForm["View"] is SimpleList<string>)
                {
                    SimpleList<string> lines = _baseForm["View"] as SimpleList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
            }
			return true;
		}

		/// <summary>
		/// Format of a branch specification used to save a branch to the server
		/// </summary>
		private static String BranchSpecFormat =
													"Branch:\t{0}\r\n" +
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
													"View:\r\n\t{6}\r\n";


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
			String value = String.Format(BranchSpecFormat, Id,
				FormBase.FormatDateTime(Updated), FormBase.FormatDateTime(Accessed),
				Owner, Description, OptionsStr, viewStr);
			return value;
		}
        #endregion

        /// <summary>
        /// Read the fields from the tagged output of a branches command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'branches' command</param>
		/// <param name="offset">Offset within array</param>
        /// <param name="dst_mismatch">Daylight savings time for conversions</param>
        public void FromBranchSpecsCmdTaggedOutput(TaggedObject objectInfo, string offset, bool dst_mismatch)
		{
			_baseForm = new FormBase();

			_baseForm.SetValues(objectInfo);

			if (objectInfo.ContainsKey("branch"))
				Id = objectInfo["branch"];

			if (objectInfo.ContainsKey("Owner"))
				Owner = objectInfo["Owner"];

			if (objectInfo.ContainsKey("Access"))
			{
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Access"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);

                Accessed = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
			}

			if (objectInfo.ContainsKey("Update"))
			{
                DateTime UTC = FormBase.ConvertUnixTime(objectInfo["Update"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);
                Updated = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
			}

			if (objectInfo.ContainsKey("Options"))
			{
				if (objectInfo["Options"] == "locked")
					Locked = true;
			}
			else
				Locked = false;

			if (objectInfo.ContainsKey("Description"))
				Description = objectInfo["Description"];

		}
		
	}
}
