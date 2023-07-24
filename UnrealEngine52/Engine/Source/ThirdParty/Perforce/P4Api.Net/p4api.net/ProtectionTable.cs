using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Specifies resource access privileges for Perforce users for a specific
	/// Perforce repository. 
	/// </summary>
	public class ProtectionTable : List<ProtectionEntry>
	{
        /// <summary>
        /// Construct ProtectionTable using a Protection Entry
        /// </summary>
        /// <param name="entry">Protection Entry</param>
	    public ProtectionTable(ProtectionEntry entry)
	    {
	        Entry = entry;
	    }

        /// <summary>
        /// Protection Table Entry
        /// </summary>
		public ProtectionEntry Entry { get; set; }
	}
}
