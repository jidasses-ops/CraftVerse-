package com.air.craftverse;

import android.app.Activity;
import android.content.Context;
import android.util.Log;
import androidx.annotation.NonNull;

import com.unity3d.ads.IUnityAdsInitializationListener;
import com.unity3d.ads.UnityAds;
import com.unity3d.services.banners.BannerView; // optional if you later use banners

/**
 * AdsBridge - simple Unity Ads bridge for native Android.
 * - Put this file in: android/app/src/main/java/com/air/craftverse/AdsBridge.java
 * - Call AdsBridge.init(activity, "YOUR_GAME_ID", true/false) from your Android activity onCreate.
 * - Call AdsBridge.showAd("placementId") from JNI/C++ when you want to display an ad.
 *
 * Notes:
 * - Replace "YOUR_GAME_ID" at runtime with your actual Unity Game ID.
 * - This implementation keeps calls on the UI thread and checks UnityAds.isReady before showing.
 */
public class AdsBridge {
    private static final String TAG = "AdsBridge";
    private static Activity sActivity;
    private static String sGameId = "";
    private static boolean sTestMode = false;
    private static boolean sInitialized = false;

    /**
     * Load native library containing JNI bridge to C++ ads code.
     * Replace "minetest" with the actual native library name from your CMake build.
     */
    static {
        try {
            System.loadLibrary("minetest");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Native library not found: " + e.getMessage());
        }
    }

    /**
     * Native method: Register Activity with JNI layer for C++ access.
     * Called from Java to provide the native layer with Activity + JavaVM references.
     */
    private static native void nativeRegister(Activity activity);

    /**
     * Initialize Unity Ads. Call once from the Android Activity (onCreate).
     * @param activity The current Activity
     * @param gameId Your Unity Game ID (per platform)
     * @param testMode true to enable test mode
     */
    public static void init(@NonNull final Activity activity, @NonNull final String gameId, final boolean testMode) {
        sActivity = activity;
        sGameId = gameId;
        sTestMode = testMode;

        if (sInitialized) {
            Log.i(TAG, "Unity Ads already initialized.");
            return;
        }

        Log.i(TAG, "Initializing Unity Ads, gameId=" + sGameId + " testMode=" + sTestMode);
        try {
            UnityAds.initialize(sActivity, sGameId, sTestMode, new IUnityAdsInitializationListener() {
                @Override
                public void onInitializationComplete() {
                    Log.i(TAG, "Unity Ads initialization complete.");
                    sInitialized = true;
                    
                    // Register native side after initialization
                    try {
                        nativeRegister(sActivity);
                        Log.i(TAG, "Native layer registered successfully");
                    } catch (UnsatisfiedLinkError e) {
                        Log.w(TAG, "nativeRegister not available: " + e.getMessage());
                    }
                }

                @Override
                public void onInitializationFailed(UnityAds.UnityAdsInitializationError error, String message) {
                    Log.w(TAG, "Unity Ads init failed: " + error + " - " + message);
                    sInitialized = false;
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Exception while initializing Unity Ads: ", e);
            sInitialized = false;
        }
    }

    /**
     * Request to show an ad with the given placement id.
     * This runs on UI thread; if not ready, it will be a no-op.
     * @param placementId placement id configured in Unity dashboard
     */
    public static void showAd(@NonNull final String placementId) {
        if (sActivity == null) {
            Log.w(TAG, "showAd: Activity is null, cannot show ad.");
            return;
        }
        sActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (!sInitialized) {
                        Log.w(TAG, "Unity Ads not initialized yet. Attempting initialization with stored gameId.");
                        if (sGameId != null && !sGameId.isEmpty()) {
                            init(sActivity, sGameId, sTestMode);
                        }
                    }
                    if (UnityAds.isReady(placementId)) {
                        Log.i(TAG, "Unity Ads is ready for placement: " + placementId + " -> showing ad.");
                        UnityAds.show(sActivity, placementId);
                    } else {
                        Log.w(TAG, "Unity Ads not ready for placement: " + placementId);
                        // Optional: you can call UnityAds.load(placementId, loadListener) here for preloading
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Exception while trying to show Unity Ad: ", e);
                }
            }
        });
    }

    /**
     * Check if a placement is ready.
     * @param placementId placement id
     * @return true if ready, false otherwise
     */
    public static boolean isReady(@NonNull final String placementId) {
        try {
            return UnityAds.isReady(placementId);
        } catch (Exception e) {
            Log.e(TAG, "isReady exception: ", e);
            return false;
        }
    }

    /**
     * Optional helper: set or update the Activity reference (call from activity onResume/onPause pairs to keep ref valid).
     */
    public static void setActivity(Activity activity) {
        sActivity = activity;
    }
}
