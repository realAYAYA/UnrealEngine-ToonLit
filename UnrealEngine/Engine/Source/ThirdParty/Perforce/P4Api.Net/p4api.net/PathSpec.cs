using System;
using System.Collections.Generic;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Describes the path and path type in a file spec.
	/// </summary>
	public abstract class PathSpec
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		protected PathSpec() { }

        /// <summary>
        /// Construct a PathSpec from a string
        /// </summary>
        /// <param name="path">path string</param>
		public PathSpec(string path)
		{
			Path = path;
		}

        /// <summary>
        /// Property to access Path string
        /// </summary>
		public string Path
		{ get; protected set; }

        /// <summary>
        /// Return a string from a PathSpec
        /// </summary>
        /// <returns>string path</returns>
		public override string ToString()
		{
			return Path;
		}

        /// <summary>
        /// Compare one PathSpec with another
        /// </summary>
        /// <param name="obj">PathSpec to compare to</param>
        /// <returns>true if equal</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj is PathSpec)
			{
				PathSpec o = obj as PathSpec;
				return ((this.GetType() == o.GetType()) &&
					(this.Path == o.Path));
			}
			return false;
		}

        /// <summary>
        /// Get HashCode for PathSpec
        /// </summary>
        /// <returns>hashcode</returns>
		public override int GetHashCode()
		{
			return Path.GetHashCode();
		}

        /// <summary>
        /// Normalize a depot Path
        /// Removes quotes around it.
        /// </summary>
        /// <param name="path">path to normalize</param>
        /// <returns>normalized path</returns>
		protected string NormalizeDepot(string path)
		{
		    string val = "";
            if (path != null)
            {
                val = path.Trim('"');
            }
			return val;
		}

        /// <summary>
        /// Normalize a Local Path
        /// Remove quotes, insure backslashes instead of forward slashes
        /// </summary>
        /// <param name="path">path to normalize</param>
        /// <returns>normalized path</returns>
		protected string NormalizeLocal(string path)
		{
			string val = "";
			if (path != null)
			{
				val = path.Trim('"', ' ');
				val = val.Replace('/', '\\');
			}		
			return val;
		}

        /// <summary>
        /// Return the filename from a path
        /// </summary>
        /// <param name="path">path to parse</param>
        /// <returns>filename</returns>
		private static string GetFileName(string path)
		{
			String name = path;
			if (!String.IsNullOrEmpty(path))
			{
				int idx = path.LastIndexOf('/');
				if (idx >= 0)
				{
					name = path.Substring(idx + 1);
				}
				else
				{
					idx = path.LastIndexOf('\\');
					if (idx >= 0)
					{
						name = path.Substring(idx + 1);
					}
				}
			}
			return name;
		}

        /// <summary>
        /// Get just the Directory path from a path
        /// Exclude the Filename
        /// </summary>
        /// <param name="path">path to parse</param>
        /// <returns>directory path</returns>
		private static string GetDirectoryName(string path)
		{
			String name = path;
			if (!String.IsNullOrEmpty(path))
			{
				int idx = path.LastIndexOf('/');
				if (idx >= 0)
				{
					name = path.Substring(0, idx);
				}
				else
				{
					idx = path.LastIndexOf('\\');
					if (idx >= 0)
					{
						name = path.Substring(0, idx);
					}
				}
			}
			return name;
		}

        /// <summary>
        /// Return the File name from this PathSpec
        /// </summary>
        /// <returns>filename</returns>
		public string GetFileName()
		{
			return GetFileName(Path);
		}

        /// <summary>
        /// Return the Path Name from 
        /// </summary>
        /// <returns></returns>
		public string GetDirectoryName()
		{
			return GetDirectoryName(Path);
		}

        /// <summary>
        /// Operator to Create a FileSpec from a PathSpec
        /// </summary>
        /// <param name="s">PathSpec</param>
        /// <returns>FileSpec</returns>
		public static implicit operator FileSpec(PathSpec s) 
		{
			return new FileSpec(s,null);
		}

		/// <summary>
		/// Unescape a string path
		/// UnEscapes the Perforce Special characters * # @ and %
		/// </summary>
		/// <param name="p">Path to unescape</param>
		/// <returns>unescaped path</returns>
		public static string UnescapePath(string p)
		{
            if (p==null)
            {
                return p;   
            }
			string v = p.Replace("%2A", "*");
			v = v.Replace("%23", "#");
			v = v.Replace("%40", "@");
			v = v.Replace("%25", "%");
			return v;
		}

		/// <summary>
		/// Escape a string path
        /// Escapes the Perforce Special characters * # @ and %
		/// </summary>
		/// <param name="p">Path to escape</param>
		/// <returns>escaped path </returns>
		public static string EscapePath(string p)
		{
            if (p == null)
            {
                return p;
            }
			string v = p.Replace("%", "%25");
			v = v.Replace("#", "%23");
			v = v.Replace("@", "%40");
			return v;
		}

		/// <summary>
		/// Escape a list of string paths.
		/// </summary>
		/// <param name="Paths">Path to escape</param>
		/// <returns>List of Escaped paths</returns>
		public static IList<string> EscapePaths(IList<string> Paths)
		{
			List<string> v = new List<string>();
			foreach (string p in Paths)
			{
				v.Add(EscapePath(p));
			}
			return v;
		}

		/// <summary>
		/// Escape an array of string paths
		/// </summary>
		/// <param name="Paths">Array of Paths to escape</param>
        /// <returns>Array of Escaped paths</returns>
		public static string[] EscapePaths(string[] Paths)
		{
			string[] v = new string[Paths.Length];
			for (int idx = 0; idx < Paths.Length; idx++)
			{
				v[idx] = EscapePath(Paths[idx]);
			}
			return v;
		}


		/// <summary>
		/// Unescape a list of string paths
		/// </summary>
		/// <param name="Paths">Paths to process</param>
		/// <returns>List of Unescaped paths</returns>
		public static IList<string> UnescapePaths(IList<string> Paths)
		{
			List<string> v = new List<string>();
			foreach (string p in Paths)
			{
				v.Add(UnescapePath(p));
			}
			return v;
		}

		/// <summary>
		/// Unescape an array of string paths
		/// </summary>
		/// <param name="Paths">Paths to unescape</param>
		/// <returns>array of unescaped path strings</returns>
		public static string[] UnescapePaths(string[] Paths)
		{
			string[] v = new string[Paths.Length];
			for (int idx = 0; idx < Paths.Length; idx++)
			{
				v[idx] = UnescapePath(Paths[idx]);
			}
			return v;
		}
	}

	/// <summary>
	/// A path spec in depot syntax. 
	/// </summary>
	public class DepotPath : PathSpec
	{
        /// <summary>
        /// Construct a DepotPath from a string path
        /// </summary>
        /// <param name="path">string path</param>
        /// <returns>A Depot Path</returns>
		public DepotPath(string path)
		{
			Path = NormalizeDepot(path);
		}
		public override bool Equals(object obj) { return base.Equals(obj); }
		public override int GetHashCode() { return base.GetHashCode(); }
	}

	/// <summary>
	/// A path spec in client syntax. 
	/// </summary>
	public class ClientPath : PathSpec
	{
        /// <summary>
        /// Construct a ClientPath from a string path
        /// </summary>
        /// <param name="path">string path</param>
        /// <returns>A Client Path</returns>
		public ClientPath(string path)
		{
			Path = NormalizeDepot(path);
		}
		public override bool Equals(object obj) { return base.Equals(obj); }
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A path spec in local syntax. 
    /// </summary>
    public class LocalPath : PathSpec
	{
        /// <summary>
        /// Construct a LocalPath from a string path
        /// </summary>
        /// <param name="path">string path</param>
        /// <returns>A Local Path</returns>
		public LocalPath(string path)
		{
			Path = NormalizeLocal(path);
		}
		public override bool Equals(object obj) { return base.Equals(obj); }
        public override int GetHashCode() { return base.GetHashCode(); }
    }

}
