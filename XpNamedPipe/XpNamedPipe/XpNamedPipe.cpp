#include "stdafx.h"

#include "XpNamedPipe.h"

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
                PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, pSA);
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

static void readBytes(HANDLE pipeHandle, char* readBuf, int bytesToRead, OVERLAPPED* overlapped) throw (std::wstring) {
    int totalBytesRead = 0;
    while (totalBytesRead < bytesToRead) {
        DWORD bytesRead = 0;
        BOOL readResult = ReadFile(pipeHandle, readBuf + totalBytesRead, bytesToRead - totalBytesRead, &bytesRead, overlapped);
        if (!readResult && GetLastError() == ERROR_IO_PENDING) {
            readResult = GetOverlappedResult(pipeHandle, overlapped, &bytesRead, TRUE);
        }
        if (!readResult) {
            throw getWindowsErrorMessage(L"ReadFile");
        }
        totalBytesRead += bytesRead;
    }
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

        DWORD bytesWritten = 0;
        int msgLength = htonl(bytesToWrite);

        BOOL writeResult = WriteFile(pipeHandle, &msgLength, sizeof(msgLength), &bytesWritten, &overlapped);
        if (!writeResult && GetLastError() == ERROR_IO_PENDING) {
            writeResult = GetOverlappedResult(pipeHandle, &overlapped, &bytesWritten, TRUE);
        }

        if (!writeResult) {
            throw getWindowsErrorMessage(L"WriteFile");
        }

        writeResult = WriteFile(pipeHandle, pipeMsg, bytesToWrite, &bytesWritten, &overlapped);
        if (!writeResult && GetLastError() == ERROR_IO_PENDING) {
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

static void readMessage(HANDLE pipeHandle, std::vector<char>& buffer) throw (std::wstring) {
    bool error = false;
    std::wstring errorMsg;

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    try {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlapped.hEvent == NULL) {
            throw getWindowsErrorMessage(L"CreateEvent");
        }

        int msgLength = 0;
        readBytes(pipeHandle, (char*)&msgLength, sizeof(msgLength), &overlapped);

        msgLength = ntohl(msgLength);

        buffer.resize(msgLength);

        readBytes(pipeHandle, &(buffer[0]), msgLength, &overlapped);
    } catch (std::wstring& tempErrorMsg) { 
        error = true;
        setErrorMessage(tempErrorMsg.c_str());
    }
    if (overlapped.hEvent != NULL) {
        CloseHandle(overlapped.hEvent);
    }
    if (error) {
        throw errorMsg;
    }
}


// Exported function definitions

std::string XPNP_getErrorMessage() {
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
    std::string result;
    try {
        result = toUtf8String(errorMessageStr);
    } catch (std::wstring) {
        result = "Failed to convert error message from UTF-16 to UTF-8";
    }
    return result;
}

bool XPNP_makePipeName(const std::string& baseName, bool userLocal, std::string& fullPipeName) {
    bool result = false;
    try {
        std::wstring fullPipeNameUtf16 = makePipeName(toUtf16String(baseName), userLocal);
        fullPipeName = toUtf8String(fullPipeNameUtf16);
        result = true;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}

XPNP_PipeHandle XPNP_createPipe(const std::string& pipeNameUtf8, bool privatePipe) {
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

bool XPNP_closePipe(XPNP_PipeHandle pipe) {
    PipeInfo* pipeInfo = (PipeInfo*)pipe;
    if (pipeInfo != NULL) {
        CloseHandle(pipeInfo->pipeHandle);
        delete pipeInfo;
    }
    return true;
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
        BOOL connectResult = ConnectNamedPipe(pipeInfo->pipeHandle, &overlapped);
        if (!connectResult && GetLastError() == ERROR_IO_PENDING) {
            connectResult = GetOverlappedResult(pipeInfo->pipeHandle, &overlapped, &unused, TRUE);
        }
        if (!connectResult && GetLastError() != ERROR_PIPE_CONNECTED) {
            throw getWindowsErrorMessage(L"ConnectNamedPipe");
        }

        std::vector<char> readBuf;
        readMessage(pipeInfo->pipeHandle, readBuf);

        std::string newPipeNameUtf8(readBuf.begin(), readBuf.end());
        std::wstring newPipeName = toUtf16String(newPipeNameUtf8);

        newPipeHandle = CreateFile((LPCWSTR)newPipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

        if (newPipeHandle == INVALID_HANDLE_VALUE) {
            throw getWindowsErrorMessage(L"CreateFile");
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

bool XPNP_readPipe(XPNP_PipeHandle pipe, std::vector<char>& buffer) {
    bool result = false;
    PipeInfo* pipeInfo = (PipeInfo*)pipe;

    try {
        readMessage(pipeInfo->pipeHandle, buffer);
        result = true;
    } catch (std::wstring& errorMsg) { 
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}

XPNP_PipeHandle XPNP_openPipe(const std::string& pipeNameUtf8, bool privatePipe) {
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
        writeMessage(listeningPipeHandle, newPipeNameUtf8.c_str(), newPipeNameUtf8.length());

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
        if (overlapped.hEvent != NULL) {
            CloseHandle(overlapped.hEvent);
        }
    }
    if (listeningPipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(listeningPipeHandle);
    }
    return (XPNP_PipeHandle)pipeInfo;
}

bool XPNP_writePipe(XPNP_PipeHandle pipe, char* pipeMsg, int bytesToWrite) {
    bool result = false;
    PipeInfo* pipeInfo = (PipeInfo*)pipe;

    try {
        writeMessage(pipeInfo->pipeHandle, pipeMsg, bytesToWrite);
        result = true;
    } catch (std::wstring& errorMsg) {
        setErrorMessage(errorMsg.c_str());
    }
    return result;
}




