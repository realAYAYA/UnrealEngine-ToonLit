// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using Microsoft.Win32.SafeHandles;

namespace Microsoft.Build.Shared.FileSystem
{
	/// <summary>
	/// Handle for a volume iteration as returned by WindowsNative.FindFirstVolumeW />
	/// </summary>
	//EPIC BEGIN
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Interoperability", "CA1419:Provide a parameterless constructor that is as visible as the containing type for concrete types derived from 'System.Runtime.InteropServices.SafeHandle'", Justification = "<Pending>")]
	//EPIC END
	internal sealed class SafeFindFileHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
		/// <summary>
		/// Private constructor for the PInvoke marshaller.
		/// </summary>
		private SafeFindFileHandle()
            : base(ownsHandle: true)
        {
        }

        /// <nodoc/>
        protected override bool ReleaseHandle()
        {
            return WindowsNative.FindClose(handle);
        }
    }
}
