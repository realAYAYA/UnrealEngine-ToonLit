package com.epicgames.ue4;

import android.app.Application;
import android.content.Context;
import android.content.res.Configuration;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.OnLifecycleEvent;
import androidx.lifecycle.ProcessLifecycleOwner;


public class GameApplication extends Application implements LifecycleObserver {
	private static final Logger Log = new Logger("UE4", "GameApp");

	private static boolean isForeground = false;

	@Override
	public void onCreate() {
		super.onCreate();


		ProcessLifecycleOwner.get().getLifecycle().addObserver(this);

	}

	@Override
	public void attachBaseContext(Context base) {
		super.attachBaseContext(base);

	}

	@Override
	public void onLowMemory() {
		super.onLowMemory();

	}

	@Override
	public void onTrimMemory(int level) {
		super.onTrimMemory(level);

	}

	@Override
	public void onConfigurationChanged (Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		
	}

	@OnLifecycleEvent(Lifecycle.Event.ON_START)
	void onEnterForeground() {
		Log.verbose("App in foreground");
		isForeground = true;
	}

	@OnLifecycleEvent(Lifecycle.Event.ON_STOP)
	void onEnterBackground() {
		Log.verbose("App in background");
		isForeground = false;
	}

	@SuppressWarnings("unused")
	public static boolean isAppInForeground() {
		return isForeground;
	}

	public static boolean isAppInBackground() {
		return !isForeground;
	}
}
