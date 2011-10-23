// XpNamedPipeJni.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "util.hpp"
#include "XpNamedPipeJni.h"
#include "XpNamedPipe.h"

using namespace util;

static boost::thread_specific_ptr<std::string> GBL_errorMessage;

const int BUF_LEN = 1024;

// Local function definitions

static void setErrorMessage(const std::string& errorMessage) {
    GBL_errorMessage.reset(new std::string(errorMessage));
}

static void throwXpnpError() {
    char buffer[BUF_LEN] = "";
    XPNP_getErrorMessage(buffer, sizeof(buffer));
    throw std::runtime_error(buffer);
}

static void checkXpnpResult(int result) {
    if (result == 0) {
        throwXpnpError();
    }
}

// Exported function definitions

jstring JNICALL Java_xpnp_XpNamedPipe_getErrorMessage(JNIEnv* pEnv, jclass cls) {
    std::wstring errorMsgUtf16;

    std::string* pErrorMsg = GBL_errorMessage.get();

    if (pErrorMsg != NULL) {
        try {
            errorMsgUtf16 = toUtf16(*pErrorMsg);
        } catch (...) {
            errorMsgUtf16 = L"Error converting error message to UTF-16";
        }
    }

    return pEnv->NewString((const jchar*)errorMsgUtf16.c_str(), (jsize)errorMsgUtf16.length());
}

jstring JNICALL Java_xpnp_XpNamedPipe_makePipeName(JNIEnv* pEnv, jclass cls, jstring javaName, jboolean userLocal) {
    jstring result = NULL;
    const jchar* pipeName = NULL;
    try {
        pipeName = pEnv->GetStringChars(javaName, NULL);
        if (pipeName == NULL) {
            throw std::bad_alloc();
        }
        char fullPipeName[256] = "";
        int xpnpResult = XPNP_makePipeName(toUtf8((wchar_t*)pipeName).c_str(), userLocal, fullPipeName, sizeof(fullPipeName));
        checkXpnpResult(xpnpResult);

        std::wstring fullPipeNameUtf16 = toUtf16(fullPipeName);
        result = pEnv->NewString((const jchar*)fullPipeNameUtf16.c_str(), (jsize)fullPipeNameUtf16.length());
    } catch (std::exception& except) {
        setErrorMessage(except.what());
    }

    if (pipeName != NULL ){
        pEnv->ReleaseStringChars(javaName, pipeName);
    }

    return result;
}

jlong JNICALL Java_xpnp_XpNamedPipe_createPipe(JNIEnv* pEnv, jclass cls, jstring javaName, 
        jboolean privatePipe) {
    const jchar* pipeName = NULL;
    XPNP_PipeHandle pipeHandle = NULL;
    try {
        pipeName = pEnv->GetStringChars(javaName, NULL);
        if (pipeName == NULL) {
            throw std::bad_alloc();
        }
        std::string pipeNameUtf8 = toUtf8((wchar_t*)pipeName);
        pipeHandle = XPNP_createPipe(pipeNameUtf8.c_str(), privatePipe);
        if (pipeHandle == NULL) {
            throwXpnpError();
        }
    } catch (std::exception& except) {
        setErrorMessage(except.what());
    }

    if (pipeName != NULL ){
        pEnv->ReleaseStringChars(javaName, pipeName);
    }
    return (jlong)(unsigned __int64)pipeHandle;
}

jboolean JNICALL Java_xpnp_XpNamedPipe_stopPipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle) {
    try {
        checkXpnpResult(XPNP_stopPipe((XPNP_PipeHandle)pipeHandle));
        return 1;
    } catch (std::exception& except) {
        setErrorMessage(except.what());
        return 0;
    }
}

jboolean JNICALL Java_xpnp_XpNamedPipe_closePipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle) {
    try {
        checkXpnpResult(XPNP_stopPipe((XPNP_PipeHandle)pipeHandle));
        return 1;
    } catch (std::exception& except) {
        setErrorMessage(except.what());
        return 0;
    }
}

jlong JNICALL Java_xpnp_XpNamedPipe_acceptConnection(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jint timeoutMsecs) {
    try {
        XPNP_PipeHandle newPipe = XPNP_acceptConnection((XPNP_PipeHandle)pipeHandle, timeoutMsecs);
        if (newPipe == NULL) {
            throwXpnpError();
        }
        return (jlong)(unsigned __int64)newPipe;
    } catch (std::exception& except) {
        setErrorMessage(except.what());
        return 0;
    }
}

jint JNICALL Java_xpnp_XpNamedPipe_readPipe(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jbyteArray readBufferJava, jint timeoutMsecs) {
    jbyte* readBuffer = NULL;
    int bytesRead = 0;
    try {
        readBuffer = pEnv->GetByteArrayElements(readBufferJava, NULL);
        if (readBuffer == NULL) {
            throw std::bad_alloc();
        }

        bytesRead = XPNP_readPipe((XPNP_PipeHandle)pipeHandle, (char*)readBuffer, pEnv->GetArrayLength(readBufferJava), timeoutMsecs);
        checkXpnpResult(bytesRead);
    } catch (std::exception& except) { 
        setErrorMessage(except.what());
    }
    if (readBuffer != NULL) {
        pEnv->ReleaseByteArrayElements(readBufferJava, readBuffer, 0);
    }
    return bytesRead;
}

jboolean JNICALL Java_xpnp_XpNamedPipe_readBytes(JNIEnv* pEnv, jclass cls, jlong pipeHandle, jbyteArray readBufferJava, jint bytesToRead, 
            jint timeoutMsecs) {
    jbyte* readBuffer = NULL;
    jboolean result = 0;
    try {
        if (bytesToRead > pEnv->GetArrayLength(readBufferJava)) {
            throw std::length_error("Buffer length less than bytesToRead");
        }
        readBuffer = pEnv->GetByteArrayElements(readBufferJava, NULL);
        if (readBuffer == NULL) {
            throw std::bad_alloc();
        }

        result = XPNP_readBytes((XPNP_PipeHandle)pipeHandle, (char*)readBuffer, bytesToRead, timeoutMsecs);
        checkXpnpResult(result);
    } catch (std::exception& except) { 
        setErrorMessage(except.what());
    }
    if (readBuffer != NULL) {
        pEnv->ReleaseByteArrayElements(readBufferJava, readBuffer, 0);
    }
    return result;
}

jlong JNICALL Java_xpnp_XpNamedPipe_openPipe(JNIEnv* pEnv, jclass cls, jstring pipeNameJava, jboolean privatePipe) {
    const jchar* pipeNameUtf16 = NULL;
    XPNP_PipeHandle newPipe = NULL;
    try {
        pipeNameUtf16 = pEnv->GetStringChars(pipeNameJava, NULL);
        if (pipeNameUtf16 == NULL) {
            throw std::bad_alloc();
        }
        std::string pipeName = toUtf8((wchar_t*)pipeNameUtf16);
        newPipe = XPNP_openPipe(pipeName.c_str(), privatePipe);
        if (newPipe == NULL) {
            throwXpnpError();
        }
    } catch (std::exception& except) {
        setErrorMessage(except.what());
    }

    if (pipeNameUtf16 != NULL) {
        pEnv->ReleaseStringChars(pipeNameJava, pipeNameUtf16);
    }

    return (jlong)(unsigned __int64)newPipe;
}

jboolean JNICALL Java_xpnp_XpNamedPipe_writePipe(JNIEnv* pEnv, jclass cls, jlong pipe, jbyteArray pipeDataJava) {
    jboolean result = 0;
    jbyte* pipeData = NULL;
    try {
        pipeData = pEnv->GetByteArrayElements(pipeDataJava, NULL);
        if (pipeData == NULL) {
            throw std::bad_alloc();
        }
        result = XPNP_writePipe((XPNP_PipeHandle)pipe, (char*)pipeData, pEnv->GetArrayLength(pipeDataJava));
        checkXpnpResult(result);
    } catch (std::exception& except) {
        setErrorMessage(except.what());
    }
    if (pipeData != NULL) {
        pEnv->ReleaseByteArrayElements(pipeDataJava, pipeData, 0);
    }
    return result;
}




