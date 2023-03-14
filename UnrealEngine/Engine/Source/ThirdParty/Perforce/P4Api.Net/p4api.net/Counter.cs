using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// A general-purpose counter or named variable in a Perforce repository. 
	/// </summary>
	public class Counter
	{
        /// <summary>
        /// Construct a counter given name and value
        /// </summary>
        /// <param name="name">counter name</param>
        /// <param name="value">counter value</param>
		public Counter
			(
			string name,
			string value
			)
		{
			Name = name;
			Value = value;
		}

        /// <summary>
        /// Get/Set counter name
        /// </summary>
		public string Name { get; set; }

        /// <summary>
        /// Get/Set counter value
        /// </summary>
		public string Value { get; set; }
	}
}
