#include "stdafx.h"

#include "XpNamedPipe.h"
#include "util.hpp"

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
        this->isStopped = false;
    }

    std::wstring pipeName;
    bool privatePipe;
    HANDLE pipeHandle;
    volatile bool isStopped;
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
    wchar_t msg[BUF_LEN] = L"";
    wchar_t buffer[BUF_LEN] = L"";
    if (FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            0,
            buffer, BUF_LEN, NULL) == 0) {
        swprintf_s(msg, L"Failed to retrieve Windows error message");       
    } else {
        swprintf_s(msg, L"%s failed with %lu: %s", funcName, error, buffer);
    }
    return std::wstring(msg);
}

char* toUtf8(const wchar_t* utf16) throw (std::wstring) {
    char* result = NULL;
    std::wstring errorMsg;
    bool error = false;
    try {
        int utf8BufLen = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
        if (utf8BufLen == 0) {
            throw std::wstring(L"Error converting string to UTF-8: ") + getWindowsErrorMessage(L"WideCharToMultiByte");
        }
        result = new char[utf8BufLen];
        if (result == NULL) {
            throw std::wstring(L"Out of memory");
        }
        int conversionResult = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, result, utf8BufLen, NULL, NULL);
        if (conversionResult == 0) {
            throw std::wstring(L"Error converting string to UTF-8: ") + getWindowsErrorMessage(L"WideCharToMultiByte");
        }
    } catch (std::wstring& tempErrorMsg) {
        errorMsg = tempErrorMsg;
        error = true;
        if (result != NULL) {
            delete [] result;
            result = NULL;
        }
    }
    if (error) {
        throw errorMsg;
    }
    return result;
}

std::string toUtf8String(const std::wstring& utf16) throw (std::wstring) {
    char* utf8 = toUtf8(utf16.c_str());
    std::string result = utf8;
    delete [] utf8;
    return result;
}

wchar_t* toUtf16(const char* utf8) throw (std::wstring) {
    wchar_t* result = NULL;
    std::wstring errorMsg;
    bool error = false;
    try {
        int utf16BufLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
        if (utf16BufLen == 0) {
            throw std::wstring(L"Error converting string to UTF-16: ") + getWindowsErrorMessage(L"MultiByteToWideChar");
        }
        result = new wchar_t[utf16BufLen];
        if (result == NULL) {
            throw std::wstring(L"Out of memory");
        }
        int conversionResult = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result, utf16BufLen);
        if (conversionResult == 0) {
            throw std::wstring(L"Error converting string to UTF-16: ") + getWindowsErrorMessage(L"MultiByteToWideChar");
        }
    } catch (std::wstring& tempErrorMsg) {
        errorMsg = tempErrorMsg;
        error = true;
        if (result != NULL) {
            delete [] result;
            result = NULL;
        }
    }
    if (error) {
        throw errorMsg;
    }
    return result;
}

std::wstring toUtf16String(const std::string& utf8) throw (std::wstring) {
    wchar_t* resultBuf = toUtf16(utf8.c_str());
    std::wstring result = resultBuf;
    delete [] resultBuf;
    return result;
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

std::wstring createUuid() throw (std::wstring) {
    UUID uuid;
    RPC_STATUS status = UuidCreate(&uuid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
        wchar_t msgBuf[BUF_LEN] = L"";
        swprintf_s(msgBuf, L"UuidCreate failed with %l", status);
        throw std::wstring(msgBuf);
    }
    RPC_WSTR uuidString = NULL;
    status = UuidToString(&uuid, &uuidString);
    if (status != RPC_S_OK) {
        wchar_t msgBuf[BUF_LEN] = L"";
        swprintf_s(msgBuf, L"UuidToString failed with %l", status);
        throw std::wstring(msgBuf);
    }
    std::wstring result = (wchar_t*)uuidString;
    RpcStringFree(&uuidString);
    return result;
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
                PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, PIPE_UNLIMITED_INSTANCES, PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, pSA);
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

static std::wstring makePipeName(const std::wstring& baseName, bool userLocal) {
    wchar_t pipeNameBuffer[256] = L"";
    if (userLocal) {
        std::wstring userSid = getUserSid();

        HRESULT hr = swprintf_s(pipeNameBuffer, L"%s%s\\%s", PIPE_PREFIX, userSid.c_str(), baseName);
        if (FAILED(hr)) {
            throw std::wstring(L"When combined with user name and prefix, pipe name is too long");
        }
    } else {
        HRESULT hr = swprintf_s(pipeNameBuffer, L"%s%s", PIPE_PREFIX, baseName);
        if (FAILED(hr)) {
            throw std::wstring(L"When combined with prefix, pipe name is too long");
        }
    }
    return std::wstring(pipeNameBuffer);
}

static void writeMessage(HANDLE pipeHandle, const char* pipeMsg, int bytesToWrite) throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    try {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }

        BOOL writeResult = WriteFile(pipeHandle, pipeMsg, bytesToWrite, NULL, &overlapped);
        if (!writeResult && GetLastError() == ERROR_IO_PENDING){
            DWORD bytesWritten = 0;
            writeResult = GetOverlappedResult(pipeHandle, &overlapped, &bytesWritten, TRUE);
        }

        if (!writeResult) {
            throw getWindowsErrorMessage(L"WriteFile");
        }
    } catch (std::wstring& tempErrorMsg) {
        error = true;
        errorMsg = tempErrorMsg;
    }

    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }

    if (error) {
        throw errorMsg;
    }
}

static void readMessage(HANDLE pipeHandle, char* buffer, int bufLen, int* bytesRead, int* bytesRemaining) throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;

    *bytesRead = 0;
    *bytesRemaining = 0;

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    try {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }

        BOOL result = ReadFile(pipeHandle, buffer, bufLen, NULL, &overlapped);
        DWORD errorCode = GetLastError();
        if (!result && errorCode != ERROR_IO_PENDING && errorCode != ERROR_MORE_DATA) {
            throw getWindowsErrorMessage(L"ReadPipe");
        }
        result = GetOverlappedResult(pipeHandle, &overlapped, (LPDWORD)bytesRead, TRUE);
        if (!result) {
            if (GetLastError() == ERROR_MORE_DATA) {
                result = PeekNamedPipe(pipeHandle, NULL, 0, NULL, NULL, (LPDWORD)bytesRemaining);
                if (!result) {
                    throw getWindowsErrorMessage(L"PeekNamedPipe");
                }
            } else {
                throw getWindowsErrorMessage(L"GetOverlappedResult");
            }
        }
    } catch (std::wstring& tempErrorMsg) { 
        error = true;
        errorMsg = tempErrorMsg;
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    if (error) {
        throw errorMsg;
    }
}

static void readEntireMessage(HANDLE pipeHandle, std::vector<char>& msg) throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;

    char* buffer = NULL;

    try {
        int bufLen = PIPE_BUF_SIZE;
        buffer = new char[bufLen];
        if (buffer == NULL) {
            throw std::wstring(L"Out of memory");
        }
        int bytesRead = 0;
        int bytesRemaining = 0;
        readMessage(pipeHandle, buffer, bufLen, &bytesRead, &bytesRemaining);
        msg.insert(msg.end(), buffer, buffer + bytesRead);
        if (bytesRemaining > 0) {
            delete [] buffer;
            buffer = new char[bytesRemaining];
            if (buffer == NULL) {
                throw std::wstring(L"Out of memory");
            }
            readMessage(pipeHandle, buffer, bytesRemaining, &bytesRead, &bytesRemaining);
            msg.insert(msg.end(), buffer, buffer + bytesRead);
        }
    } catch (std::wstring tempErrorMsg) {
        error = true;
        errorMsg = tempErrorMsg;
    }
    if (buffer != NULL) {
        delete [] buffer;
    }
    
    if (error) {
        throw errorMsg;
    }
}

// Exported function definitions

void XPNP_getErrorMessage(char* buffer, int bufLen) {
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
    std::string utf8;
    try {
        utf8 = toUtf8String(errorMessageStr);
    } catch (std::wstring) {
        utf8 = "Failed to convert error message from UTF-16 to UTF-8";
    }
    if (strcpy_s(buffer, bufLen, utf8.c_str())) {
        strcpy_s(buffer, bufLen, "Buffer too small");
    }
}

int XPNP_makePipeName(const char* baseName, bool userLocal, char* pipeNameBuf, int bufLen) {
    bool result = 0;
    try {
        std::wstring fullPipeNameUtf16 = makePipeName(toUtf16String(baseName), userLocal);
        std::string fullPipeName = toUtf8String(fullPipeNameUtf16);
        if (strcpy_s(pipeNameBuf, bufLen, fullPipeName.c_str()) != 0) {
            throw std::wstring(L"Buffer too small");
        }
        result = 1;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}

XPNP_PipeHandle XPNP_createPipe(const char* pipeNameUtf8, bool privatePipe) {
    PipeInfo* pipeInfo = NULL;
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    try {
        std::wstring pipeName = toUtf16String(pipeNameUtf8);
        pipeHandle = createPipe(pipeName, privatePipe != false);
        pipeInfo = new PipeInfo(pipeName, privatePipe != false, pipeHandle);
        if (pipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
        }
    }

    return (XPNP_PipeHandle)pipeInfo;
}

int XPNP_stopPipe(XPNP_PipeHandle pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    if (! pipeInfo->isStopped) {
        CloseHandle(pipeInfo->pipeHandle);
        pipeInfo->isStopped = true;
    }
    return 1;
}

int XPNP_deletePipe(XPNP_PipeHandle pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    if (!pipeInfo->isStopped) {
        CloseHandle(pipeInfo->pipeHandle);
    }
    delete pipeInfo;
    return 1;
}

XPNP_PipeHandle XPNP_acceptConnection(XPNP_PipeHandle pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    HANDLE newPipeHandle = INVALID_HANDLE_VALUE;
    PipeInfo* newPipeInfo = NULL;    
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    try {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }
        
        DWORD unused = 0;
        BOOL result = ConnectNamedPipe(pipeInfo->pipeHandle, &overlapped);
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            result = GetOverlappedResult(pipeInfo->pipeHandle, &overlapped, &unused, TRUE);
        }
        if (!result && GetLastError() != ERROR_PIPE_CONNECTED) {
            throw getWindowsErrorMessage(L"ConnectNamedPipe");
        }

        std::vector<char> readBuf;
        readEntireMessage(pipeInfo->pipeHandle, readBuf);

        std::string newPipeNameUtf8(readBuf.begin(), readBuf.end());
        std::wstring newPipeName = toUtf16String(newPipeNameUtf8);

        newPipeHandle = CreateFile((LPCWSTR)newPipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

        if (newPipeHandle == INVALID_HANDLE_VALUE) {
            throw getWindowsErrorMessage(L"CreateFile");
        }

        DWORD mode = PIPE_READMODE_MESSAGE;
        result = SetNamedPipeHandleState(newPipeHandle, &mode, NULL, NULL);
        if (!result) {
            throw getWindowsErrorMessage(L"SetNamedPipeHandleState");
        }

        newPipeInfo = new PipeInfo(newPipeName, pipeInfo->privatePipe, newPipeHandle);
        if (newPipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (newPipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(newPipeHandle);
        }
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    return (XPNP_PipeHandle)newPipeInfo;
}

int XPNP_readMessage(XPNP_PipeHandle pipe, char* buffer, int bufLen, int* bytesRead, int* bytesRemaining) {
    int result = 0;
    PipeInfo* pipeInfo = (PipeInfo*)pipe;

    try {
        readMessage(pipeInfo->pipeHandle, buffer, bufLen, bytesRead, bytesRemaining);
        result = 1;
    } catch (std::wstring& errorMsg) { 
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}

XPNP_PipeHandle XPNP_openPipe(const char* pipeNameUtf8, bool privatePipe) {
    PipeInfo* pipeInfo = NULL;
    HANDLE listeningPipeHandle = INVALID_HANDLE_VALUE;
    HANDLE newPipeHandle = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    try {
        std::wstring pipeName = toUtf16String(pipeNameUtf8);
        int tries = 0; 
        DWORD createError = 0;
        do {
            listeningPipeHandle = CreateFile((LPCWSTR)pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
            tries++;
            if (listeningPipeHandle == INVALID_HANDLE_VALUE && tries < 5) {
                createError = GetLastError();
                if (createError == ERROR_PIPE_BUSY) {
                    Sleep(100);
                }
            }
        } while (listeningPipeHandle == INVALID_HANDLE_VALUE && tries < 5 && createError == ERROR_PIPE_BUSY);

        if (listeningPipeHandle == INVALID_HANDLE_VALUE) {
            throw getWindowsErrorMessage(L"CreateFile");
        }

        std::wstring newPipeName = makePipeName(createUuid(), false);

        newPipeHandle = createPipe(newPipeName, privatePipe);

        std::string newPipeNameUtf8 = toUtf8String(newPipeName);
        writeMessage(listeningPipeHandle, newPipeNameUtf8.c_str(), (int)newPipeNameUtf8.length());

        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }
        DWORD unused = 0;
        BOOL connectResult = ConnectNamedPipe(newPipeHandle, &overlapped);
        if (!connectResult && GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(overlapped.hEvent, 1000) != WAIT_OBJECT_0) {
                throw getWindowsErrorMessage(L"WaitForSingleObject");
            }
            connectResult = GetOverlappedResult(newPipeHandle, &overlapped, &unused, FALSE);
        }
        if (!connectResult && GetLastError() != ERROR_PIPE_CONNECTED) {
            throw getWindowsErrorMessage(L"ConnectNamedPipe");
        }

        pipeInfo = new PipeInfo(newPipeName, privatePipe, newPipeHandle);
        if (pipeInfo == NULL) {
            throw std::wstring(L"Out of memory");
        }
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
        if (newPipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(newPipeHandle);
        }
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    if (listeningPipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(listeningPipeHandle);
    }
    return (XPNP_PipeHandle)pipeInfo;
}

int XPNP_writeMessage(XPNP_PipeHandle pipe, const char* pipeMsg, int bytesToWrite) {
    int result = 0;
    PipeInfo* pipeInfo = (PipeInfo*)pipe;

    try {
        writeMessage(pipeInfo->pipeHandle, pipeMsg, bytesToWrite);
        result = 1;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}




