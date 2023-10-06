// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal.network;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.epicgames.unreal.Logger;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class NetworkChangedManager implements NetworkConnectivityClient {

	private enum ConnectivityState {
		CONNECTION_AVAILABLE,
		NO_CONNECTION
	}

	private static final int MAX_RETRY_SEC = 13;
	private int currentHostResolutionAddressIndex = 0;
	private static final String[] HOST_RESOLUTION_ADDRESSES = new String[] {
		"https://example.com/",
		"https://google.com/",
		"https://www.samsung.com/"
	};
	private static final long HOSTNAME_RESOLUTION_TIMEOUT_MS = 2000;

	private static final Logger Log = new Logger("UE", "NetworkChangedManager");

	private static NetworkChangedManager instance;
	@NonNull
	public static synchronized NetworkConnectivityClient getInstance() {
		if (instance == null) {
			instance = new NetworkChangedManager();
		}
		return instance;
	}

	/*
	 * References
	 */
	private ConnectivityManager connectivityManager;

	@NonNull
	private Set<WeakReference<Listener>> networkChangedListeners = new HashSet<>();

	/*
	 * Data
	 */
	private Set<String> networks = new HashSet<>();
	@Nullable
	private ConnectivityState currentState = null;
	@Nullable
	private NetworkTransportType currentNetworkTransport = NetworkTransportType.UNKNOWN;

	private boolean networkCheckInProgress = false;
	private boolean retryScheduled = false;
	private int retryCount = 0;

	private Handler internalScheduler = new Handler();

	private NetworkChangedManager() {
	}

	@Override
	public void initNetworkCallback(@NonNull Context context) {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
			// unsupported before Lollipop
			return;
		}

		connectivityManager = (ConnectivityManager) context.getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
		if (connectivityManager == null) {
			Log.error("Unable to start connectivityManager");
			return;
		}
		NetworkRequest.Builder builder = new NetworkRequest.Builder();
		builder.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
		builder.addTransportType(NetworkCapabilities.TRANSPORT_WIFI);
		builder.addTransportType(NetworkCapabilities.TRANSPORT_ETHERNET);
		builder.addTransportType(NetworkCapabilities.TRANSPORT_VPN);
		builder.addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
		builder.addTransportType(NetworkCapabilities.TRANSPORT_BLUETOOTH);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			builder.addCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
		}
		connectivityManager.registerNetworkCallback(builder.build(), connectivityListener);
	}

	@TargetApi(Build.VERSION_CODES.LOLLIPOP)
	private ConnectivityManager.NetworkCallback connectivityListener = new ConnectivityManager.NetworkCallback() {
		@Override
		public void onAvailable(Network network) {
			networks.add(network.toString());
			Log.verbose("Network Available: " + network.toString());
			checkNetworkConnectivity();
		}

		@Override
		public void onLost(Network network) {
			networks.remove(network.toString());
			Log.verbose("Network Lost callback: " + network.toString());

			if (networks.isEmpty()) {
				Log.verbose("All Networks Lost");
			} else {
				Log.verbose("Not All Networks Lost");
			}
			checkNetworkConnectivity();
		}

		@Override
		public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
					&& !networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) {
				Log.verbose("Network Capabilities changed, doesn't have validated net_capability");
				networks.remove(network.toString());
			} else if (!networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) {
				Log.verbose("Network Capabilities changed, has Internet: " + false);
				networks.remove(network.toString());
			} else {
				Log.verbose("Network Capabilities changed, has Internet: " + true);
				networks.add(network.toString());
			}
			checkNetworkConnectivity();
		}
	};

	private Runnable retryRunnable = new Runnable() {
		@Override
		public void run() {
			Log.verbose("Attempting to check for network connectivity again.");
			retryCount++;
			retryScheduled = false;
			checkNetworkConnectivity();
		}
	};

	private void scheduleRetry() {
		if (networkChangedListeners.size() == 0) {
			Log.verbose("No listeners so not retrying. When a listener is added the connection will be checked.");
			return;
		}
		if (retryScheduled || networkCheckInProgress) {
			return;
		}
		retryScheduled = true;
		internalScheduler.removeCallbacksAndMessages(retryRunnable);
		internalScheduler.postDelayed(retryRunnable, calculateRetryDelay());
	}

	/**
	 * @return a delay in MS that increases exponentially but with a max value of
	 * {@link NetworkChangedManager#MAX_RETRY_SEC} seconds
	 */
	private long calculateRetryDelay() {
		return (long) (Math.min(MAX_RETRY_SEC, Math.pow(2, retryCount)) * 1000);
	}

	private void clearRetry() {
		internalScheduler.removeCallbacksAndMessages(retryRunnable);
		retryCount = 0;
		retryScheduled = false;
	}

	private void setNetworkState(ConnectivityState state) {
        NetworkTransportType updatedNetworkTransportType = calculateNetworkTransport(connectivityManager);
        if (currentState == state && currentNetworkTransport == updatedNetworkTransportType) {
			Log.verbose("Connectivity hasn't changed. Current state: " + currentState);
			if (currentState != ConnectivityState.CONNECTION_AVAILABLE) {
				scheduleRetry();
			}
			return;
		}
		currentState = state;
        currentNetworkTransport = updatedNetworkTransportType;
		fireNetworkChangeListeners(state, currentNetworkTransport);
		Log.verbose("Network connectivity changed. New connectivity state: " + state);

		if (currentState != ConnectivityState.CONNECTION_AVAILABLE) {
			scheduleRetry();
		} else {
			clearRetry();
		}
	}

	private void checkNetworkConnectivity() {
		ConnectivityState naiveNetworkState = calculateNetworkConnectivityNaively();
		if (naiveNetworkState != ConnectivityState.CONNECTION_AVAILABLE) {
			setNetworkState(ConnectivityState.NO_CONNECTION);
			return;
		} else if (currentState == null) {
			Log.verbose("No network state set yet, setting naive network state checking connection fully.");
			setNetworkState(naiveNetworkState);
		}

		if (networkCheckInProgress) {
			return;
		}

		networkCheckInProgress = true;
		final ExecutorService executor = Executors.newSingleThreadExecutor();
		final Runnable timeoutRunnable = new Runnable() {
			@Override
			public void run() {
				Log.verbose("Unable to connect to: " + getCurrentHostResolutionAddress());
				networkCheckInProgress = false;
				executor.shutdownNow();
				setNetworkState(ConnectivityState.NO_CONNECTION);
				scheduleRetry();
			}
		};
		internalScheduler.postDelayed(timeoutRunnable, HOSTNAME_RESOLUTION_TIMEOUT_MS * HOST_RESOLUTION_ADDRESSES.length + 100);
		executor.execute(new Runnable() {
			@Override
			public void run() {
				HttpURLConnection urlConnection = null;
				boolean connectedSuccessfully = false;
				// Attempt to connect to any of the hostnames. If any succeed we are connected to
				// the internet.
				for (int i = 0; i < HOST_RESOLUTION_ADDRESSES.length; i++) {
					try {
						Log.verbose("Verifying internet connection with host: " + getCurrentHostResolutionAddress());
						URL url = new URL(getCurrentHostResolutionAddress());
						urlConnection = (HttpURLConnection) url.openConnection();
						urlConnection.setUseCaches(false);
						urlConnection.setRequestMethod("HEAD");
						urlConnection.setConnectTimeout((int) HOSTNAME_RESOLUTION_TIMEOUT_MS);
						urlConnection.setReadTimeout((int) HOSTNAME_RESOLUTION_TIMEOUT_MS);
						urlConnection.getInputStream().close();
						connectedSuccessfully = true;
						break;
					} catch (MalformedURLException e) {
						Log.error("Malformed URL, this should never happen. Please fix, url: " + getCurrentHostResolutionAddress());
					} catch (IOException e) {
						Log.verbose("Unable to connect to: " + getCurrentHostResolutionAddress());
					} catch (Exception e) {
						Log.verbose("Unable to connect to: " + getCurrentHostResolutionAddress() + ", exception: " + e.toString());
					} finally {
						if (urlConnection != null) {
							urlConnection.disconnect();
						}
						if (!connectedSuccessfully) {
							/*
							 * Move to the next resolution address.
							 * When we try again we should do it with the next available URL. For
							 * some reason on some networks we've had issues with
							 * https://example.com so using https://google.com and others will give
							 * us more redundancy
							 */
							nextHostResolutionAddress();
						}
					}
				}

				internalScheduler.removeCallbacks(timeoutRunnable);
				networkCheckInProgress = false;
				if (connectedSuccessfully) {
					setNetworkState(ConnectivityState.CONNECTION_AVAILABLE);
				} else {
					setNetworkState(ConnectivityState.NO_CONNECTION);
					scheduleRetry();
				}
				Log.verbose("Full network check complete. State: " + currentState);

				executor.shutdownNow();
			}
		});
	}

	@NonNull
	private ConnectivityState calculateNetworkConnectivityNaively() {
		if (networks.isEmpty() || connectivityManager == null) {
			return ConnectivityState.NO_CONNECTION;
		} else {
			NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();

			if (activeNetworkInfo != null && activeNetworkInfo.isAvailable() && activeNetworkInfo.isConnected()) {
				return ConnectivityState.CONNECTION_AVAILABLE;
			} else {
				return ConnectivityState.NO_CONNECTION;
			}
		}
	}

	private String getCurrentHostResolutionAddress() {
		return HOST_RESOLUTION_ADDRESSES[currentHostResolutionAddressIndex];
	}

	private void nextHostResolutionAddress() {
		if (currentHostResolutionAddressIndex + 1 >= HOST_RESOLUTION_ADDRESSES.length) {
			currentHostResolutionAddressIndex = 0;
		} else {
			currentHostResolutionAddressIndex += 1;
		}
	}

	@Override
	public boolean addListener(Listener listener) {
		return addListener(listener, false);
	}

	@Override
	public boolean addListener(Listener listener, boolean fireImmediately) {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
			// unsupported before Lollipop
			return false;
		}

		for (WeakReference<Listener> currentListener : networkChangedListeners) {
			if (currentListener.get() == listener) {
				return false;
			}
		}
		networkChangedListeners.add(new WeakReference<>(listener));
		if (networkChangedListeners.size() == 1) {
			/*
			 * Start checking connectivity when we go from 0 -> 1 listeners.
			 * When no listeners are registered checking connectivity is automatically stopped.
			 */
			checkNetworkConnectivity();
		}
		// Current state will never be null after calling checkNetworkConnectivity
		if (fireImmediately && currentState != null) {
			fireNetworkChangeListenerInternal(listener, currentState, currentNetworkTransport);
		}
		return true;
	}

	@Override
	public boolean removeListener(Listener listener) {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
			// unsupported before Lollipop
			return false;
		}

		Iterator<WeakReference<Listener>> changeListenersIterator = networkChangedListeners.iterator();
		while (changeListenersIterator.hasNext()) {
			WeakReference<Listener> currentListener = changeListenersIterator.next();
			if (currentListener.get() == listener) {
				changeListenersIterator.remove();
				return true;
			}
		}
		/*
		 * If there are no listeners, clear any retries. When a listener is added we'll check the
		 * connection again.
		 */
		if (networkChangedListeners.size() == 0) {
			clearRetry();
		}
		return false;
	}

	private void fireNetworkChangeListeners(ConnectivityState state, NetworkTransportType networkTransportType) {
		Iterator<WeakReference<Listener>> changeListenersIterator = networkChangedListeners.iterator();
		while (changeListenersIterator.hasNext()) {
			WeakReference<Listener> listenerWeakReference = changeListenersIterator.next();
			Listener listener = listenerWeakReference.get();
			if (listener == null) {
				changeListenersIterator.remove();
			} else {
				fireNetworkChangeListenerInternal(listener, state, networkTransportType);
			}
		}
	}

	private void fireNetworkChangeListenerInternal(Listener listener, ConnectivityState state, NetworkTransportType networkTransportType) {
		switch (state) {
			case NO_CONNECTION:
				listener.onNetworkLost();
				break;
			case CONNECTION_AVAILABLE:
				listener.onNetworkAvailable(networkTransportType);
				break;
		}
	}

	@Override
	public NetworkTransportType networkTransportTypeCheck()
	{
		return calculateNetworkTransport(connectivityManager);
	}
	
	@Override
	public void checkConnectivity() {
		checkNetworkConnectivity();
	}

    private NetworkTransportType calculateNetworkTransport(@NonNull ConnectivityManager connectivityManager) {
       if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
           NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
           if (networkInfo == null) {
               return NetworkTransportType.UNKNOWN;
           }

           switch (networkInfo.getType()) {
               case ConnectivityManager.TYPE_WIFI:
                   return NetworkTransportType.WIFI;
               case ConnectivityManager.TYPE_MOBILE:
                   return NetworkTransportType.CELLULAR;
               case ConnectivityManager.TYPE_ETHERNET:
                   return NetworkTransportType.ETHERNET;
               case ConnectivityManager.TYPE_BLUETOOTH:
                   return NetworkTransportType.BLUETOOTH;
               case ConnectivityManager.TYPE_VPN:
                   return NetworkTransportType.VPN;
               default:
                   return NetworkTransportType.UNKNOWN;
           }
       } else {
           Network activeNetwork = connectivityManager.getActiveNetwork();
           if (activeNetwork == null) {
               return NetworkTransportType.UNKNOWN;
           }
           NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(activeNetwork);
           if (capabilities == null) {
               return NetworkTransportType.UNKNOWN;
           }

           if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
               return NetworkTransportType.WIFI;
           } else if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
               return NetworkTransportType.CELLULAR;
           } else if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) {
               return NetworkTransportType.ETHERNET;
           } else if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH)) {
               return NetworkTransportType.BLUETOOTH;
           } else if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
               return NetworkTransportType.VPN;
           } else {
               return NetworkTransportType.UNKNOWN;
           }
       }
   }
}
