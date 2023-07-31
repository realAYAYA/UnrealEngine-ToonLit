// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class ActivationListener : IDisposable
	{
		Thread? _backgroundThread;
		EventWaitHandle _activateEventHandle;
		EventWaitHandle _quitEventHandle;

		public event Action? OnActivate;

		public ActivationListener(EventWaitHandle inActivateEventHandle)
		{
			_activateEventHandle = inActivateEventHandle;
			_quitEventHandle = new EventWaitHandle(false, EventResetMode.ManualReset);
		}

		public void Start()
		{
			if(_backgroundThread == null)
			{
				_backgroundThread = new Thread(x => ThreadProc());
				_backgroundThread.IsBackground = true;
				_backgroundThread.Start();
			}
		}

		public void Stop()
		{
			if(_backgroundThread != null)
			{
				_quitEventHandle.Set();

				_backgroundThread.Join();
				_backgroundThread = null;
			}
		}

		public void Dispose()
		{
			Stop();

			_quitEventHandle.Dispose();
			_activateEventHandle.Dispose();
		}

		void ThreadProc()
		{
			for(;;)
			{
				int index = EventWaitHandle.WaitAny(new WaitHandle[]{ _activateEventHandle, _quitEventHandle }, Timeout.Infinite);
				if(index == 0)
				{
					OnActivate?.Invoke();
				}
				else
				{
					break;
				}
			}
		}
	}
}
