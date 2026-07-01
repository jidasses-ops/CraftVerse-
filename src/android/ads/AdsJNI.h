#pragma once

#include <jni.h>
#include <string>

namespace craftverse {
namespace ads {

/**
 * Initialize the wrapper with a JavaVM pointer (optional if you call registerActivity).
 * Call initWithJavaVM if you obtain JavaVM* elsewhere.
 */
void initWithJavaVM(JavaVM* vm);

/**
 * Register the Android Activity instance (JNI env + activity object).
 * Call this from Java (AdsBridge.nativeRegister(activity)) or from native init code.
 */
void registerActivity(JNIEnv* env, jobject activity);

/**
 * Set the Unity Game id (optional, C++ side only).
 */
void setGameId(const std::string &gameId, bool testMode);

/**
 * Show an ad for the given placement id (non-blocking, posts to UI thread).
 */
void showAd(const std::string &placement);

/**
 * Return whether a placement is ready to show (calls UnityAds.isReady on Java side).
 */
bool isReady(const std::string &placement);

/**
 * Clean-up references (call on shutdown if needed).
 */
void cleanup();

} // namespace ads
} // namespace craftverse
