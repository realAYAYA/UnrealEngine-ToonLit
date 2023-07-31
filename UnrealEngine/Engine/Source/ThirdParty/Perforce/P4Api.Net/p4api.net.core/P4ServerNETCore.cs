using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.Versioning;
using System.Text;

namespace Perforce.P4
{
    public partial class P4Server : IDisposable
    {


        /// <summary>
        /// Get the version of the .NET Framework
        /// </summary>
        public static String NETVersion => Assembly.GetEntryAssembly()?.GetCustomAttribute<TargetFrameworkAttribute>()?.FrameworkName;

    }
}
