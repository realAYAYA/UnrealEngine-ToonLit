// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	class RichTextBox41 : RichTextBox
	{
		[DllImport("kernel32.dll", CharSet = CharSet.Auto)]
		static extern IntPtr LoadLibrary(string lpFileName);

		protected override CreateParams CreateParams
		{
			get
			{
				CreateParams p = base.CreateParams;
				if (LoadLibrary("msftedit.dll") != IntPtr.Zero)
				{
					p.ClassName = "RICHEDIT50W";
				}
				return p;
			}
		}
	}
}
