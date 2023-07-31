// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum TaskbarState
	{
		NoProgress = 0,
		Indeterminate = 0x1,
		Normal = 0x2,
		Error = 0x4,
		Paused = 0x8
	}

	static class Taskbar
	{
		[ComImport, Guid("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
		interface ITaskbarList3
		{
			void HrInit();
			void AddTab(IntPtr hWnd);
			void DeleteTab(IntPtr hWnd);
			void ActivateTab(IntPtr hWnd);
			void SetActiveAlt(IntPtr hWnd);
			void MarkFullscreenWindow(IntPtr hWnd, int fFullscreen);
			void SetProgressValue(IntPtr hWnd, UInt64 ullCompleted, UInt64 ullTotal);
			void SetProgressState(IntPtr hWnd, TaskbarState state);
		}

		[ComImport, Guid("56FDF344-FD6D-11d0-958A-006097C9A090"), ClassInterfaceAttribute(ClassInterfaceType.None)]
		class TaskbarList3
		{
		}

		static ITaskbarList3? _interface;

		static Taskbar()
		{
			if(Environment.OSVersion.Version >= new Version(6, 1))
			{
				_interface = new TaskbarList3() as ITaskbarList3;
			}
		}

		public static void SetState(IntPtr windowHandle, TaskbarState state)
		{
			if(_interface != null)
			{
				try
				{
					_interface.SetProgressState(windowHandle, state);
				}
				catch
				{
				}
			}
		}

		public static void SetProgress(IntPtr windowHandle, ulong completed, ulong total)
		{
			if(_interface != null)
			{
				try
				{
					_interface.SetProgressValue(windowHandle, completed, total);
				}
				catch
				{
				}
			}
		}
	}
}
