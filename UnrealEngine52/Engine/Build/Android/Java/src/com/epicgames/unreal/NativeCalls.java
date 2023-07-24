package com.epicgames.unreal;

public class NativeCalls
{
	@SuppressWarnings("JniMissingFunction")
	public static native void HandleCustomTouchEvent(int deviceId, int pointerId, int action, int source, float x, float y);

	@SuppressWarnings("JniMissingFunction")
	public static native void AllowJavaBackButtonEvent(boolean allow);

	@SuppressWarnings("JniMissingFunction")
	public static native void CallNativeToEmbedded(String ID, int Priority, String Subsystem, String Command, String[] Params, String RoutingFunction);

	@SuppressWarnings("JniMissingFunction")
	public static native void SetNamedObject(String Name, Object Obj);

	@SuppressWarnings("JniMissingFunction")
	public static native void KeepAwake(String Requester, boolean bIsForRendering);

	@SuppressWarnings("JniMissingFunction")
	public static native void AllowSleep(String Requester);

	@SuppressWarnings("JniMissingFunction")
	public static native void WebViewVisible(boolean bShown);

	@SuppressWarnings("JniMissingFunction")
	public static native void ForwardNotification(String payload);

	@SuppressWarnings("JniMissingFunction")
	public static native void RouteServiceIntent(String action, String payload);

	@SuppressWarnings("JniMissingFunction")
	public static native void UELogError(String msg);

	@SuppressWarnings("JniMissingFunction")
	public static native void UELogWarning(String msg);

	@SuppressWarnings("JniMissingFunction")
	public static native void UELogLog(String msg);

	@SuppressWarnings("JniMissingFunction")
	public static native void UELogVerbose(String msg);
}