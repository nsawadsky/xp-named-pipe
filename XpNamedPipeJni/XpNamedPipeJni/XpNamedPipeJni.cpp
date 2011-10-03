// XpNamedPipeJni.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "XpNamedPipeJni.h"

const int BUF_LEN = 1024;

static DWORD GBL_errorMessageTlsIndex = TlsAlloc();

const wchar_t* PIPE_PREFIX = L"\\\\.\\pipe\\";

const int PIPE_BUF_SIZE = 10 * 1024;

// Type definitions
class PipeInfo {
public:
    PipeInfo(const std::wstring& pipeName, bool privatePipe, HANDLE pipeHandle) {
        this->pipeName = pipeName;
        this->privatePipe = privatePipe;
        this->pipeHandle = pipeHandle;
    }

    std::wstring pipeName;
    bool privatePipe;
    HANDLE pipeHandle;
};

// Local function definitions

static bool setErrorMessage(const wchar_t* errorMessage) {
    wchar_t* currMessage = (wchar_t*)TlsGetValue(GBL_errorMessageTlsIndex);
    if (currMessage != NULL) {
        if (TlsSetValue(GBL_errorMessageTlsIndex, NULL)) {
            delete [] currMessage;
        } else {
            return false;
        }
    }
    if (errorMessage == NULL) {
        return true;
    } else {
        int bufSize = (int)wcslen(errorMessage) + 1;
        wchar_t* newMessage = new wchar_t[bufSize];
        if (newMessage == NULL) {
            return false;
        } else {
            wcscpy_s(newMessage, bufSize, errorMessage);
            if (TlsSetValue(GBL_errorMessageTlsIndex, newMessage)) {
                return true;
            } else {
                delete [] newMessage;
                return false;
            }
        }
    }
}

static std::wstring getWindowsErrorMessage(wchar_t* funcName) {
    DWORD error = GetLastError();
    wchar_t buffer[BUF_LEN] = L"";
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer, BUF_LEN, NULL);
    wchar_t msg[BUF_LEN] = L"";
    swprintf_s(msg, L"%s failed with %lu: %s", funcName, error, buffer);
    return std::wstring(msg);
}

static std::wstring getUserSid() throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;
    HANDLE tokenHandle = NULL;
    PTOKEN_USER pUserInfo = NULL;
    wchar_t* pSidString = NULL;
    std::wstring sid;
    try {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
            throw getWindowsErrorMessage(L"OpenThreadToken");
        }
        DWORD userInfoSize = 0;
        GetTokenInformation(tokenHandle, TokenUser, NULL, 0, &userInfoSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            throw getWindowsErrorMessage(L"GetTokenInformation");
        }
        pUserInfo = (PTOKEN_USER)malloc(userInfoSize);
        if (pUserInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
        if (!GetTokenInformation(tokenHandle, TokenUser, pUserInfo, userInfoSize, &userInfoSize)) {
            throw getWindowsErrorMessage(L"GetTokenInformation");
        }
        if (!ConvertSidToStringSid(pUserInfo->User.Sid, &pSidString)) {
            throw getWindowsErrorMessage(L"ConvertSidToStringSid");
        }
        sid = pSidString;
            
    } catch (std::wstring& tempErrorMsg) {
        error = true;
        errorMsg = tempErrorMsg;
    }
    if (tokenHandle != NULL) {
        CloseHandle(tokenHandle);
    }
    if (pUserInfo != NULL) {
        free(pUserInfo);
    }
    if (pSidString != NULL) {
        LocalFree(pSidString);
    }
    if (error) {
        throw errorMsg;
    }
    return sid;
}

static HANDLE createPipe(const std::wstring& pipeName, bool privatePipe) throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES* pSA = NULL;
    try {
        if (privatePipe) {
            pSA = new SECURITY_ATTRIBUTES;
            if (pSA == NULL) {
                throw std::wstring(L"Out of memory");
            }
            pSA->nLength = sizeof(SECURITY_ATTRIBUTES);
            pSA->bInheritHandle = FALSE;
            pSA->lpSecurityDescriptor = NULL;
            std::wstring userSid = getUserSid();

            // Allow GA access (generic all, i.e. full control) to the local system account, builtin administrators,
            // and the current user's SID.  Inheritance is disabled (I don't think inheritance has any meaning in the context of
            // pipes).  Because there is no inheritance, there is need to mention CO (creator owner).  CO represents the SID of a 
            // user who creates a new object underneath a given object.  Deny access to processes on other machines 
            // (whose token contains NU, the network logon user SID).
            std::wstring sddl = L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;";
            sddl.append(userSid);
            sddl.append(L")(D;;GA;;;NU)");
            if (!ConvertStringSecurityDescriptorToSecurityDescriptor(sddl.c_str(), SDDL_REVISION_1, &(pSA->lpSecurityDescriptor), NULL)) {
                throw getWindowsErrorMessage(L"ConvertStringSecurityDescriptorToSecurityDescriptor");
            }
        }
        pipeHandle = CreateNamedPipe(pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES, PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, pSA);
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            throw getWindowsErrorMessage(L"CreateNamedPipe");
        }
    } catch (std::wstring& tempErrorMsg) {
        error = true;
        errorMsg = tempErrorMsg;
    }

    if (pSA != NULL) {
        if (pSA->lpSecurityDescriptor != NULL) {
            LocalFree(pSA->lpSecurityDescriptor);
        }
        delete pSA;
    }
    if (error) {
        throw errorMsg;
    }
    return pipeHandle;
}

// Exported function definitions

jstring JNICALL Java_xpnp_XpNamedPipe_getErrorMessage(JNIEnv* pEnv, jclass cls) {
    std::wstring errorMessageStr;
    wchar_t* errorMessage = (wchar_t*)TlsGetValue(GBL_errorMessageTlsIndex);
    if (errorMessage == NULL) {
        if (GetLastError() != ERROR_SUCCESS) {
            errorMessageStr = L"Failed to retrieve error message: ";
            errorMessageStr += getWindowsErrorMessage(L"TlsGetValue");
        }
    } else {
        errorMessageStr = errorMessage;
    }
    return pEnv->NewString((const jchar*)errorMessageStr.c_str(), (jsize)errorMessageStr.length());
}

jstring JNICALL Java_xpnp_XpNamedPipe_makePipeName(JNIEnv* pEnv, jclass cls, jstring javaName, jboolean userLocal) {
    jstring result = NULL;
    const jchar* pipeName = NULL;
    wchar_t fullPipeName[256] = L"";
    try {
        pipeName = pEnv->GetStringChars(javaName, NULL);
        if (pipeName == NULL) {
            throw std::wstring(L"JNI out of memory");
        }
        if (userLocal) {
            std::wstring userSid = getUserSid();

            HRESULT hr = swprintf_s(fullPipeName, L"%s%s\\%s", PIPE_PREFIX, userSid.c_str(), pipeName);
            if (FAILED(hr)) {
                throw std::wstring(L"When combined with user name and prefix, pipe name is too long");
            }
        } else {
            HRESULT hr = swprintf_s(fullPipeName, L"%s%s", PIPE_PREFIX, pipeName);
            if (FAILED(hr)) {
                throw std::wstring(L"When combined with prefix, pipe name is too long");
            }
        }
        result = pEnv->NewString((const jchar*)fullPipeName, (jsize)wcslen(fullPipeName));
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }

    if (pipeName != NULL ){
        pEnv->ReleaseStringChars(javaName, pipeName);
    }

    return result;
}

jlong JNICALL Java_xpnp_XpNamedPipe_createPipe(JNIEnv* pEnv, jclass cls, jstring javaName, 
        jboolean privatePipe) {
    PipeInfo* pipeInfo = NULL;
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    const jchar* pipeName = NULL;
    try {
        pipeName = pEnv->GetStringChars(javaName, NULL);
        if (pipeName == NULL) {
            throw std::wstring(L"JNI out of memory");
        }
        std::wstring pipeNameStr = std::wstring((wchar_t*)pipeName);
        pipeHandle = createPipe(pipeNameStr, privatePipe != FALSE);
        pipeInfo = new PipeInfo(pipeNameStr, privatePipe != FALSE, pipeHandle);
        if (pipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
        }
    }

    if (pipeName != NULL ){
        pEnv->ReleaseStringChars(javaName, pipeName);
    }
    return (jlong)(unsigned __int64)pipeInfo;
}

jboolean JNICALL Java_xpnp_XpNamedPipe_closePipe(JNIEnv* pEnv, jclass cls, jlong pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    if (pipeInfo != NULL) {
        CloseHandle(pipeInfo->pipeHandle);
        delete pipeInfo;
    }
    return TRUE;
}

jlong JNICALL Java_xpnp_XpNamedPipe_acceptConnection(JNIEnv* pEnv, jclass cls, jlong pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    HANDLE newPipeHandle = INVALID_HANDLE_VALUE;
    PipeInfo* newPipeInfo = NULL;    
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    try {
        // Create new listening pipe
        newPipeHandle = createPipe(pipeInfo->pipeName, pipeInfo->privatePipe);

        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }
        
        DWORD unused = 0;
        BOOL connectResult = ConnectNamedPipe(pipeInfo->pipeHandle, &overlapped);
        if (!connectResult && GetLastError() == ERROR_IO_PENDING) {
            connectResult = GetOverlappedResult(pipeInfo->pipeHandle, &overlapped, &unused, TRUE);
        }
        if (!connectResult && GetLastError() != ERROR_PIPE_CONNECTED) {
            throw getWindowsErrorMessage(L"ConnectNamedPipe");
        }

        // The new pipe info receives the existing pipe handle
        newPipeInfo = new PipeInfo(pipeInfo->pipeName, pipeInfo->privatePipe, pipeInfo->pipeHandle);
        if (newPipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
        // The old pipe info receives the new listening pipe handle
        pipeInfo->pipeHandle = newPipeHandle;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (newPipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(newPipeHandle);
        }
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    return (jlong)(unsigned __int64)newPipeInfo;
}

jobject JNICALL Java_xpnp_XpNamedPipe_readPipe(JNIEnv* pEnv, jclass cls, jlong pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    std::vector<unsigned char> returnBuf;

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    jbyteArray resultArray = NULL;
    unsigned char* readBuffer = NULL;
    int readBufSize = PIPE_BUF_SIZE;
    try {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }

        BOOL result = FALSE;
        while (!result) {
            if (readBuffer != NULL) {
                delete [] readBuffer;
            }
            readBuffer = new unsigned char[readBufSize];
            if (readBuffer == NULL) {
                throw std::wstring(L"Out of memory");
            }

            DWORD bytesRead = 0;
            result = ReadFile(pipeInfo->pipeHandle, readBuffer, readBufSize, &bytesRead, &overlapped);

            if (!result && GetLastError() == ERROR_IO_PENDING) {
                result = GetOverlappedResult(pipeInfo->pipeHandle, &overlapped, &bytesRead, TRUE);
            }

            if (result || (!result && GetLastError() == ERROR_MORE_DATA)) {
                returnBuf.insert(returnBuf.end(), readBuffer, readBuffer + bytesRead);
            } 

            if (!result) {
                if (GetLastError() == ERROR_MORE_DATA) {
                    DWORD bytesRemaining = 0;
                    if (!PeekNamedPipe(pipeInfo->pipeHandle, NULL, 0, NULL, NULL, &bytesRemaining)) {
                        throw getWindowsErrorMessage(L"PeekNamedPipe");
                    }
                    readBufSize = bytesRemaining;
                } else {
                    throw getWindowsErrorMessage(L"ReadFile");
                }
            }
        }
        resultArray = pEnv->NewByteArray((jsize)returnBuf.size());
        if (resultArray == NULL) {
            throw std::wstring(L"JNI out of memory");
        }
        pEnv->SetByteArrayRegion(resultArray, 0, (jsize)returnBuf.size(), (jbyte*)returnBuf.data());
    } catch (std::wstring& errorMsg) { 
        setErrorMessage(errorMsg.c_str());
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    if (readBuffer != NULL) {
        delete [] readBuffer;
    }
    return resultArray;
}

jlong JNICALL Java_xpnp_XpNamedPipe_openPipe(JNIEnv* pEnv, jclass cls, jstring pipeNameJava) {
    const jchar* pipeName = NULL;
    PipeInfo* pipeInfo = NULL;
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    try {
        pipeName = pEnv->GetStringChars(pipeNameJava, NULL);
        if (pipeName == NULL) {
            throw std::wstring(L"Out of memory");
        }
        int tries = 0; 
        DWORD createError = 0;
        do {
            pipeHandle = CreateFile((LPCWSTR)pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
            tries++;
            if (pipeHandle == INVALID_HANDLE_VALUE && tries < 5) {
                createError = GetLastError();
                if (createError == ERROR_PIPE_BUSY) {
                    Sleep(100);
                }
            }
        } while (pipeHandle == INVALID_HANDLE_VALUE && tries < 5 && createError == ERROR_PIPE_BUSY);

        if (pipeHandle == INVALID_HANDLE_VALUE) {
            throw getWindowsErrorMessage(L"CreateFile");
        }

        DWORD mode = PIPE_READMODE_MESSAGE;
        if (! SetNamedPipeHandleState(pipeHandle, &mode, NULL, NULL)) {
            throw getWindowsErrorMessage(L"SetNamedPipeHandleState");
        }
        pipeInfo = new PipeInfo(std::wstring((wchar_t*)pipeName), true, pipeHandle);
        if (pipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
        }
    }
    if (pipeName != NULL) {
        pEnv->ReleaseStringChars(pipeNameJava, pipeName);
    }
    return (jlong)(unsigned __int64)pipeInfo;
}

jboolean JNICALL Java_xpnp_XpNamedPipe_writePipe(JNIEnv* pEnv, jclass cls, jlong pipe, jbyteArray pipeMsgJava) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    jboolean result = FALSE;
    jbyte* pipeMsg = NULL;
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    try {
        pipeMsg = pEnv->GetByteArrayElements(pipeMsgJava, NULL);
        if (pipeMsg == NULL) {
            throw std::wstring(L"JNI out of memory");
        }
        jsize bytesToWrite = pEnv->GetArrayLength(pipeMsgJava);
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }
        DWORD bytesWritten = 0;
        BOOL writeResult = WriteFile(pipeInfo->pipeHandle, pipeMsg, bytesToWrite, &bytesWritten, &overlapped);
        if (!writeResult && GetLastError() == ERROR_IO_PENDING) {
            writeResult = GetOverlappedResult(pipeInfo->pipeHandle, &overlapped, &bytesWritten, TRUE);
        }

        if (!writeResult) {
            throw getWindowsErrorMessage(L"WriteFile");
        }
        result = TRUE;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    if (pipeMsg != NULL) {
        pEnv->ReleaseByteArrayElements(pipeMsgJava, pipeMsg, 0);
    }
    return result;
}




