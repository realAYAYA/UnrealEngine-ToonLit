// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal.network;

import android.content.Context;
import androidx.annotation.NonNull;

public interface NetworkConnectivityClient {
    public enum NetworkTransportType {
        WIFI,
        VPN,
        ETHERNET,
        CELLULAR,
        BLUETOOTH,
        UNKNOWN
    }

    public interface Listener {
        void onNetworkAvailable(NetworkTransportType networkTransportType);

		void onNetworkLost();
	}

	void initNetworkCallback(@NonNull Context context);

	/**
	 * See {@link NetworkConnectivityClient#addListener(Listener, boolean)}
	 */
	boolean addListener(Listener listener);

	/**
	 * @param listener The listener to add. Will be stored as a weak reference so a hard reference
	 *                 must be saved externally.
	 * @param fireImmediately Whether to trigger the listener with the current network state
	 *                        immediately after adding.
	 * @return Whether the change listener was added. Will be false if already registered.
	 */
	boolean addListener(Listener listener, boolean fireImmediately);

	/**
	 * Remove a given listener.
	 * @return Whether the change listener was removed. Will be false if not currently registered.
	 */
	boolean removeListener(Listener listener);
	
	/**
	 * Check for network connectivity
	 */
	void checkConnectivity();

	/**
	 * Check the current type of network connection
	 * @return Type of network connection the device currently have
	 */
	NetworkTransportType networkTransportTypeCheck();
}
