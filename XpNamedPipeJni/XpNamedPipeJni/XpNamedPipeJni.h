#pragma once

#ifdef __cplusplus
extern "C" {
#endif

jstring JNICALL Java_xpnp_XpNamedPipe_getErrorMessage(JNIEnv* pEnv, jclass cls);

jstring JNICALL Java_xpnp_XpNamedPipe_makePipeName(JNIEnv* pEnv, jclass cls, jstring javaName, jboolean userLocal);

jlong JNICALL Java_xpnp_XpNamedPipe_createPipe(JNIEnv* pEnv, jclass cls, jstring javaName, jboolean privatePipe);

jboolean JNICALL Java_xpnp_XpNamedPipe_stopPipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle);

jboolean JNICALL Java_xpnp_XpNamedPipe_closePipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle);

jlong JNICALL Java_xpnp_XpNamedPipe_acceptConnection(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jint timeoutMsecs);

jint JNICALL Java_xpnp_XpNamedPipe_readPipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jbyteArray buffer, jint timeoutMsecs);

jboolean JNICALL Java_xpnp_XpNamedPipe_readBytes(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jbyteArray buffer, jint bytesToRead, jint timeoutMsecs);

jlong JNICALL Java_xpnp_XpNamedPipe_openPipe(JNIEnv* pEnv, jclass cls, jstring pipeNameJava, jboolean privatePipe);

jboolean JNICALL Java_xpnp_XpNamedPipe_writePipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jbyteArray pipeDataJava);

#ifdef __cplusplus
}
#endif
