#include "stdafx.h"

#include "XpNamedPipe.h"
#include "util.hpp"
using namespace util;

// Impl based on http://msdn.microsoft.com/en-us/library/windows/desktop/aa365603(v=vs.85).aspx

const int PIPE_BUF_SIZE = 10 * 1024;

// Globals
static boost::thread_specific_ptr<ErrorInfo> GBL_errorInfo;

// Type definitions

class PipeInfo {
public:
    PipeInfo(const std::string& pipeName, bool privatePipe, HANDLE pipeHandle) : 
            pipeName(pipeName), privatePipe(privatePipe), pipeHandle(pipeHandle) {

        stoppedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        stoppedEvent.check("CreateEvent");
    }

    HANDLE getPipeHandle() {
        return pipeHandle;
    }

    HANDLE getStoppedEvent() {
        return stoppedEvent;
    }

    const std::string& getName() {
        return pipeName;
    }

    bool isPrivatePipe() {
        return privatePipe;
    }

    void stop() {
        checkWindowsResult(SetEvent(stoppedEvent), "SetEvent");
    }

private:
    std::string pipeName;
    bool privatePipe;
    ScopedFileHandle pipeHandle;
    ScopedHandle stoppedEvent;
};

// Local function definitions

static void setErrorInfo(const std::string& errorMessage, int errorCode = 0) {
    GBL_errorInfo.reset(new ErrorInfo(errorMessage, errorCode));
}

static void setErrorInfo(const ErrorInfo& info) {
    GBL_errorInfo.reset(new ErrorInfo(info));
}

static std::string getUserSid() {
    bool error = false;
    std::exception except;

    HANDLE tokenHandle = NULL;
    PTOKEN_USER pUserInfo = NULL;
    char* pSidString = NULL;
    std::string sid;
    try {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
            throwWindowsError("OpenThreadToken");
        }
        DWORD userInfoSize = 0;
        GetTokenInformation(tokenHandle, TokenUser, NULL, 0, &userInfoSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            throwWindowsError("GetTokenInformation");
        }
        pUserInfo = (PTOKEN_USER)malloc(userInfoSize);
        if (pUserInfo == NULL) {
            throw std::bad_alloc();
        }
        if (!GetTokenInformation(tokenHandle, TokenUser, pUserInfo, userInfoSize, &userInfoSize)) {
            throwWindowsError("GetTokenInformation");
        }
        if (!ConvertSidToStringSidA(pUserInfo->User.Sid, &pSidString)) {
            throwWindowsError("ConvertSidToStringSid");
        }
        sid = pSidString;
            
    } catch (std::exception& e) {
        error = true;
        except = e;
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
        throw except;
    }
    return sid;
}

static std::string createUuid() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::stringstream stream;
    stream << uuid;
    return stream.str();
}

static HANDLE createPipe(const std::string& pipeName, bool privatePipe) {
    bool error = false;
    std::exception except;

    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES* pSA = NULL;
    try {
        if (privatePipe) {
            pSA = new SECURITY_ATTRIBUTES;

            pSA->nLength = sizeof(SECURITY_ATTRIBUTES);
            pSA->bInheritHandle = FALSE;
            pSA->lpSecurityDescriptor = NULL;
            std::string userSid = getUserSid();

            // Allow GA access (generic all, i.e. full control) to the local system account, builtin administrators,
            // and the current user's SID.  Inheritance is disabled (I don't think inheritance has any meaning in the context of
            // pipes).  Because there is no inheritance, there is need to mention CO (creator owner).  CO represents the SID of a 
            // user who creates a new object underneath a given object.  Deny access to processes on other machines 
            // (whose token contains NU, the network logon user SID).
            std::string sddl = "D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;";
            sddl.append(userSid);
            sddl.append(")(D;;GA;;;NU)");
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &(pSA->lpSecurityDescriptor), NULL)) {
                throwWindowsError("ConvertStringSecurityDescriptorToSecurityDescriptor");
            }
        }
        pipeHandle = CreateNamedPipe(toUtf16(pipeName).c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, pSA);
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            throwWindowsError("CreateNamedPipe");
        }
    } catch (std::exception& e) {
        error = true;
        except = e;
    }

    if (pSA != NULL) {
        if (pSA->lpSecurityDescriptor != NULL) {
            LocalFree(pSA->lpSecurityDescriptor);
        }
        delete pSA;
    }
    if (error) {
        throw except;
    }
    return pipeHandle;
}

static std::string makePipeName(const std::string& baseName, bool userLocal) {
    std::stringstream pipeName;
    const char* PIPE_PREFIX = "\\\\.\\pipe\\";

    if (userLocal) {
        pipeName << PIPE_PREFIX << getUserSid() << "\\" << baseName;
    } else {
        pipeName << PIPE_PREFIX << baseName;
    }
    return pipeName.str();
}

static void writeBytes(HANDLE pipeHandle, const char* pipeMsg, int bytesToWrite) {
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    ScopedHandle evt = overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    evt.check("CreateEvent");

    DWORD bytesWritten = 0;
    BOOL writeResult = WriteFile(pipeHandle, pipeMsg, bytesToWrite, &bytesWritten, &overlapped);
    if (!writeResult && GetLastError() == ERROR_IO_PENDING){
        writeResult = GetOverlappedResult(pipeHandle, &overlapped, &bytesWritten, TRUE);
    }

    checkWindowsResult(writeResult, "WriteFile");
}

static int readPipe(PipeInfo* pipeInfo, char* buffer, int bufLen, int timeoutMsecs) {
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));

    ScopedHandle evt = overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    evt.check("CreateEvent");

    int bytesRead = 0;
    BOOL result = ReadFile(pipeInfo->getPipeHandle(), buffer, bufLen, (LPDWORD)&bytesRead, &overlapped);
    DWORD errorCode = GetLastError();
    if (!result && errorCode != ERROR_IO_PENDING) {
        throwWindowsError("ReadFile");
    }
    if (GetLastError() == ERROR_IO_PENDING) {
        HANDLE handles[2] = {pipeInfo->getStoppedEvent(), evt};
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, timeoutMsecs);
        if (waitResult == WAIT_FAILED || waitResult == WAIT_TIMEOUT || waitResult == WAIT_OBJECT_0) {
            std::string errorMsg = getWindowsErrorMessage("WaitForMultipleObjects");
            CancelIo(pipeInfo->getPipeHandle());

            DWORD unused = 0;
            GetOverlappedResult(pipeInfo->getPipeHandle(), &overlapped, &unused, TRUE);

            if (waitResult == WAIT_FAILED) {
                throw std::runtime_error(errorMsg);
            } else if (waitResult == WAIT_OBJECT_0) {
                throw std::runtime_error("Interrupted while reading message");
            } else {
                throw ErrorInfo("Timed out while reading message", XPNP_ERROR_TIMEOUT);
            }
        }
        result = GetOverlappedResult(pipeInfo->getPipeHandle(), &overlapped, (LPDWORD)&bytesRead, TRUE);
        checkWindowsResult(result, "GetOverlappedResult");
    }
    return bytesRead;
}

static void readBytes(PipeInfo* pipeInfo, char* buffer, int bytesToRead, int timeoutMsecs) {
    int totalBytesRead = 0;
    while (totalBytesRead < bytesToRead) {
        totalBytesRead += readPipe(pipeInfo, buffer + totalBytesRead, bytesToRead - totalBytesRead, timeoutMsecs);
    }
}

static void writeMessage(HANDLE pipeHandle, const char* msg, int msgLen) {
    int msgLenNetwork = htonl(msgLen);
    writeBytes(pipeHandle, (const char*)&msgLenNetwork, sizeof(msgLenNetwork));
    writeBytes(pipeHandle, msg, msgLen);
}

static void readMessage(PipeInfo* pipeInfo, std::vector<char>& msg, int timeoutMsecs){
    int msgLen = 0;
    readBytes(pipeInfo, (char*)&msgLen, sizeof(msgLen), timeoutMsecs);
    msgLen = ntohl(msgLen);

    if (msgLen > 0) {
        msg.resize(msgLen);
        readBytes(pipeInfo, &(msg[0]), msgLen, timeoutMsecs);
    }
}

static PipeInfo* getPipeInfo(XPNP_PipeHandle handle) {
    if (handle == 0) {
        throw std::invalid_argument("Pipe handle is null");
    }
    return (PipeInfo*)handle;
}

// Exported function definitions

void XPNP_getErrorMessage(char* buffer, int bufLen) {
    ErrorInfo* pErrorInfo = GBL_errorInfo.get();
    const char* errorMsg = "";
    if (pErrorInfo != NULL) {
        errorMsg = pErrorInfo->what();
    }
    if (strcpy_s(buffer, bufLen, errorMsg) != 0) {
        strcpy_s(buffer, bufLen, "Buffer too small");
    }
}

int XPNP_getErrorCode() {
    ErrorInfo* pErrorInfo = GBL_errorInfo.get();
    int errorCode = 0;
    if (pErrorInfo != NULL) {
        errorCode = pErrorInfo->getErrorCode();
    }
    return errorCode;
}

int XPNP_makePipeName(const char* baseName, int userLocal, char* pipeNameBuf, int bufLen) {
    try {
        std::string fullPipeName = makePipeName(baseName, userLocal != 0);
        if (strcpy_s(pipeNameBuf, bufLen, fullPipeName.c_str()) != 0) {
            throw std::runtime_error("Buffer too small");
        }
        return 1;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        return 0;
    }
}

XPNP_PipeHandle XPNP_createPipe(const char* pipeName, int privatePipe) {
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    try {
        pipeHandle = createPipe(pipeName, privatePipe != 0);
        return (XPNP_PipeHandle)new PipeInfo(pipeName, privatePipe != 0, pipeHandle);
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
        }
        return NULL;
    }
}

int XPNP_stopPipe(XPNP_PipeHandle pipe) {
    try {
        PipeInfo* pipeInfo = getPipeInfo(pipe);
        pipeInfo->stop();
        return 1;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        return 0;
    }
}

int XPNP_closePipe(XPNP_PipeHandle pipe) {
    try {
        PipeInfo* pipeInfo = getPipeInfo(pipe);
        delete pipeInfo;
        return 1;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        return 0;
    }
}

XPNP_PipeHandle XPNP_acceptConnection(XPNP_PipeHandle pipe, int timeoutMsecs) {
    HANDLE newPipeHandle = INVALID_HANDLE_VALUE;
    PipeInfo* pipeInfo = NULL;
    PipeInfo* newPipeInfo = NULL;    
    bool connectAttempted = false;
    bool errorOccurred = false;

    try {
        pipeInfo = getPipeInfo(pipe);

        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));

        ScopedHandle evt = overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        evt.check("CreateEvent");
        
        BOOL result = ConnectNamedPipe(pipeInfo->getPipeHandle(), &overlapped);
        connectAttempted = true;

        if (!result && GetLastError() == ERROR_IO_PENDING) {
            DWORD unused = 0;
            HANDLE handles[2] = {pipeInfo->getStoppedEvent(), evt};
            DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, timeoutMsecs);
            if (waitResult == WAIT_FAILED || waitResult == WAIT_TIMEOUT || waitResult == WAIT_OBJECT_0) {
                std::string errorMsg = getWindowsErrorMessage("WaitForMultipleObjects");
                CancelIo(pipeInfo->getPipeHandle());
                GetOverlappedResult(pipeInfo->getPipeHandle(), &overlapped, &unused, TRUE);
                if (waitResult == WAIT_FAILED) {
                    throw std::runtime_error(errorMsg);
                } else if (waitResult == WAIT_OBJECT_0) {
                    throw std::runtime_error("Interrupted while waiting for client to connect");
                } else {
                    throw ErrorInfo("Timed out waiting for client to connect", XPNP_ERROR_TIMEOUT);
                }
            }
            result = GetOverlappedResult(pipeInfo->getPipeHandle(), &overlapped, &unused, TRUE);
        }
        if (!result && GetLastError() != ERROR_PIPE_CONNECTED) {
            throwWindowsError("ConnectNamedPipe");
        }

        std::vector<char> readBuf;
        readMessage(pipeInfo, readBuf, timeoutMsecs);

        std::string newPipeName(readBuf.begin(), readBuf.end());

        newPipeHandle = CreateFile(toUtf16(newPipeName).c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

        if (newPipeHandle == INVALID_HANDLE_VALUE) {
            throwWindowsError("CreateFile");
        }

        newPipeInfo = new PipeInfo(newPipeName, pipeInfo->isPrivatePipe(), newPipeHandle);
    } catch (ErrorInfo& info) {
        setErrorInfo(info);
        errorOccurred = true;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        errorOccurred = true;
    }
    if (errorOccurred) {
        if (newPipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(newPipeHandle);
        }
    }
    if (connectAttempted) {
        DisconnectNamedPipe(pipeInfo->getPipeHandle());
    }
    return (XPNP_PipeHandle)newPipeInfo;
}

int XPNP_readPipe(XPNP_PipeHandle pipe, char* buffer, int bufLen, int timeoutMsecs) {
    try {
        if (bufLen <= 0) {
            throw std::invalid_argument("bufLen <= 0");
        }
        PipeInfo* pipeInfo = getPipeInfo(pipe);
        return readPipe(pipeInfo, buffer, bufLen, timeoutMsecs);
    } catch (ErrorInfo& e) {
        setErrorInfo(e);
        return 0;
    } catch (std::exception& e) { 
        setErrorInfo(e.what());
        return 0;
    }
}

int XPNP_readBytes(XPNP_PipeHandle pipe, char* buffer, int bytesToRead, int timeoutMsecs) {
    try {
        if (bytesToRead <= 0) {
            throw std::invalid_argument("bytesToRead <= 0");
        }
        PipeInfo* pipeInfo = getPipeInfo(pipe);
        readBytes(pipeInfo, buffer, bytesToRead, timeoutMsecs);
        return 1;
    } catch (ErrorInfo& e) {
        setErrorInfo(e);
        return 0;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        return 0;
    }
}

XPNP_PipeHandle XPNP_openPipe(const char* pipeName, int privatePipe) {
    PipeInfo* pipeInfo = NULL;
    HANDLE newPipeHandle = INVALID_HANDLE_VALUE;
    try {
        ScopedFileHandle listeningPipeHandle;

        int tries = 0; 
        DWORD createError = 0;

        const int MAX_TRIES = 15;
        std::wstring pipeNameUtf16 = toUtf16(pipeName);
        do {
            listeningPipeHandle = CreateFile(pipeNameUtf16.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
            tries++;
            if (listeningPipeHandle == INVALID_HANDLE_VALUE && tries < MAX_TRIES) {
                createError = GetLastError();
                if (createError == ERROR_PIPE_BUSY) {
                    Sleep(100);
                }
            }
        } while (listeningPipeHandle == INVALID_HANDLE_VALUE && tries < MAX_TRIES && createError == ERROR_PIPE_BUSY);

        listeningPipeHandle.check("CreateFile");

        std::string newPipeName = makePipeName(createUuid(), false);

        newPipeHandle = createPipe(newPipeName, privatePipe != 0);

        writeMessage(listeningPipeHandle, newPipeName.c_str(), (int)newPipeName.length());

        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));

        ScopedHandle evt = overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        evt.check("CreateEvent");

        BOOL connectResult = ConnectNamedPipe(newPipeHandle, &overlapped);
        if (!connectResult && GetLastError() == ERROR_IO_PENDING) {
            DWORD unused = 0;
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 2000);
            if (waitResult == WAIT_FAILED || waitResult == WAIT_TIMEOUT) {
                std::string errorMsg = getWindowsErrorMessage("WaitForSingleObject");
                CancelIo(pipeInfo->getPipeHandle());
                GetOverlappedResult(pipeInfo->getPipeHandle(), &overlapped, &unused, TRUE);
                if (waitResult == WAIT_FAILED) {
                    throw std::runtime_error(errorMsg);
                } else {
                    throw std::runtime_error("Timed out waiting for server to connect");
                }
            }
            connectResult = GetOverlappedResult(newPipeHandle, &overlapped, &unused, TRUE);
        }
        if (!connectResult && GetLastError() != ERROR_PIPE_CONNECTED) {
            throwWindowsError("ConnectNamedPipe");
        }

        pipeInfo = new PipeInfo(newPipeName, privatePipe != 0, newPipeHandle);
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        if (newPipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(newPipeHandle);
        }
    }
    return (XPNP_PipeHandle)pipeInfo;
}

int XPNP_writePipe(XPNP_PipeHandle pipe, const char* data, int bytesToWrite) {
    try {
        if (bytesToWrite <= 0) {
            throw std::invalid_argument("bytesToWrite <= 0");
        }
        PipeInfo* pipeInfo = getPipeInfo(pipe);
        writeBytes(pipeInfo->getPipeHandle(), data, bytesToWrite);
        return 1;
    } catch (std::exception& e) {
        setErrorInfo(e.what());
        return 0;
    }
}




