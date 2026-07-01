#include "AdsJNI.h"
#include <mutex>
#include <android/log.h>

static const char* LOGTAG = "AdsJNI";

using namespace craftverse::ads;

namespace {

JavaVM* gJvm = nullptr;
jobject gActivityGlobal = nullptr;
jclass gAdsBridgeClass = nullptr;

jmethodID gInitMethod = nullptr;
jmethodID gShowAdMethod = nullptr;
jmethodID gIsReadyMethod = nullptr;
jmethodID gSetActivityMethod = nullptr;

std::mutex gMutex;
std::string gGameId;
bool gTestMode = false;

static JNIEnv* getJNIEnv(bool &mustDetach) {
    mustDetach = false;
    if (gJvm == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, LOGTAG, "getJNIEnv: gJvm is null");
        return nullptr;
    }
    JNIEnv* env = nullptr;
    jint r = gJvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (r == JNI_OK) {
        return env;
    }
    if (r == JNI_EDETACHED) {
        if (gJvm->AttachCurrentThread(&env, nullptr) == 0) {
            mustDetach = true;
            return env;
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOGTAG, "Failed to AttachCurrentThread");
            return nullptr;
        }
    }
    __android_log_print(ANDROID_LOG_ERROR, LOGTAG, "Failed to get JNIEnv (error %d)", r);
    return nullptr;
}

static jclass loadAdsBridgeClass(JNIEnv* env) {
    jclass local = env->FindClass("com/air/craftverse/AdsBridge");
    if (!local) {
        __android_log_print(ANDROID_LOG_ERROR, LOGTAG, "Failed to FindClass com/air/craftverse/AdsBridge");
        return nullptr;
    }
    jclass global = (jclass)env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
}

static bool ensureMethodIDs(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gAdsBridgeClass) {
        gAdsBridgeClass = loadAdsBridgeClass(env);
        if (!gAdsBridgeClass) return false;
    }

    if (!gInitMethod)
        gInitMethod = env->GetStaticMethodID(gAdsBridgeClass, "init", "(Landroid/app/Activity;Ljava/lang/String;Z)V");
    if (!gShowAdMethod)
        gShowAdMethod = env->GetStaticMethodID(gAdsBridgeClass, "showAd", "(Ljava/lang/String;)V");
    if (!gIsReadyMethod)
        gIsReadyMethod = env->GetStaticMethodID(gAdsBridgeClass, "isReady", "(Ljava/lang/String;)Z");
    if (!gSetActivityMethod)
        gSetActivityMethod = env->GetStaticMethodID(gAdsBridgeClass, "setActivity", "(Landroid/app/Activity;)V");

    if (!gInitMethod) __android_log_print(ANDROID_LOG_WARN, LOGTAG, "gInitMethod not found");
    if (!gShowAdMethod) __android_log_print(ANDROID_LOG_WARN, LOGTAG, "gShowAdMethod not found");
    if (!gIsReadyMethod) __android_log_print(ANDROID_LOG_WARN, LOGTAG, "gIsReadyMethod not found");
    if (!gSetActivityMethod) __android_log_print(ANDROID_LOG_WARN, LOGTAG, "gSetActivityMethod not found");

    return true;
}

} // anonymous namespace

namespace craftverse {
namespace ads {

void initWithJavaVM(JavaVM* vm) {
    std::lock_guard<std::mutex> lock(gMutex);
    gJvm = vm;
    __android_log_print(ANDROID_LOG_INFO, LOGTAG, "initWithJavaVM: JavaVM stored");
}

void registerActivity(JNIEnv* env, jobject activity) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gActivityGlobal) {
        env->DeleteGlobalRef(gActivityGlobal);
        gActivityGlobal = nullptr;
    }
    gActivityGlobal = env->NewGlobalRef(activity);
    __android_log_print(ANDROID_LOG_INFO, LOGTAG, "registerActivity: activity global ref set");

    // Make sure we have the class and method IDs
    if (!ensureMethodIDs(env)) {
        __android_log_print(ANDROID_LOG_ERROR, LOGTAG, "registerActivity: could not ensure method ids");
        return;
    }

    // Optionally call Java init from native side if we have a stored game id
    if (!gGameId.empty() && gInitMethod) {
        jstring jgame = env->NewStringUTF(gGameId.c_str());
        env->CallStaticVoidMethod(gAdsBridgeClass, gInitMethod, gActivityGlobal, jgame, (jboolean)gTestMode);
        env->DeleteLocalRef(jgame);
        __android_log_print(ANDROID_LOG_INFO, LOGTAG, "registerActivity: called AdsBridge.init from native side");
    } else {
        __android_log_print(ANDROID_LOG_INFO, LOGTAG, "registerActivity: no gameId set or init method missing");
    }
}

void setGameId(const std::string &gameId, bool testMode) {
    std::lock_guard<std::mutex> lock(gMutex);
    gGameId = gameId;
    gTestMode = testMode;
    __android_log_print(ANDROID_LOG_INFO, LOGTAG, "setGameId: set game id and testmode");
}

void showAd(const std::string &placement) {
    bool mustDetach = false;
    JNIEnv* env = getJNIEnv(mustDetach);
    if (!env) return;

    if (!ensureMethodIDs(env)) {
        if (mustDetach) gJvm->DetachCurrentThread();
        return;
    }

    if (!gShowAdMethod) {
        __android_log_print(ANDROID_LOG_WARN, LOGTAG, "showAd: showAd method not resolved");
        if (mustDetach) gJvm->DetachCurrentThread();
        return;
    }

    jstring jplacement = env->NewStringUTF(placement.c_str());
    env->CallStaticVoidMethod(gAdsBridgeClass, gShowAdMethod, jplacement);
    env->DeleteLocalRef(jplacement);

    if (mustDetach) gJvm->DetachCurrentThread();
}

bool isReady(const std::string &placement) {
    bool mustDetach = false;
    JNIEnv* env = getJNIEnv(mustDetach);
    if (!env) return false;

    if (!ensureMethodIDs(env) || !gIsReadyMethod) {
        if (mustDetach) gJvm->DetachCurrentThread();
        return false;
    }

    jstring jplacement = env->NewStringUTF(placement.c_str());
    jboolean res = env->CallStaticBooleanMethod(gAdsBridgeClass, gIsReadyMethod, jplacement);
    env->DeleteLocalRef(jplacement);

    if (mustDetach) gJvm->DetachCurrentThread();
    return (res == JNI_TRUE);
}

void cleanup() {
    bool mustDetach = false;
    JNIEnv* env = getJNIEnv(mustDetach);
    if (!env) return;
    std::lock_guard<std::mutex> lock(gMutex);
    if (gActivityGlobal) {
        env->DeleteGlobalRef(gActivityGlobal);
        gActivityGlobal = nullptr;
    }
    if (gAdsBridgeClass) {
        env->DeleteGlobalRef(gAdsBridgeClass);
        gAdsBridgeClass = nullptr;
    }
    if (mustDetach) gJvm->DetachCurrentThread();
    __android_log_print(ANDROID_LOG_INFO, LOGTAG, "cleanup done");
}

} // namespace ads
} // namespace craftverse

// JNI method that Java can call to register the Activity and ensure JavaVM is stored.
// Declaration in Java (AdsBridge): private static native void nativeRegister(Activity activity);
extern "C"
JNIEXPORT void JNICALL
Java_com_air_craftverse_AdsBridge_nativeRegister(JNIEnv* env, jclass /*clazz*/, jobject activity) {
    // store JavaVM if not yet stored
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) == 0 && vm != nullptr) {
        initWithJavaVM(vm);
    }
    registerActivity(env, activity);
}
