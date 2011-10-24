#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct XPNP_Pipe {};
typedef XPNP_Pipe* XPNP_PipeHandle;

void XPNP_getErrorMessage(char* buffer, int bufLen);

int XPNP_makePipeName(const char* baseName, int userLocal, char* pipeNameBuf, int bufLen);
 
XPNP_PipeHandle XPNP_createPipe(const char* pipeName, int privatePipe);

int XPNP_stopPipe(XPNP_PipeHandle pipeHandle);

int XPNP_deletePipe(XPNP_PipeHandle pipeHandle);

XPNP_PipeHandle XPNP_acceptConnection(XPNP_PipeHandle pipeHandle, int timeoutMsecs);

int XPNP_readPipe(XPNP_PipeHandle pipeHandle, char* buffer, int bufLen, int timeoutMsecs);

int XPNP_readBytes(XPNP_PipeHandle pipeHandle, char* buffer, int bytesToRead, int timeoutMsecs);

XPNP_PipeHandle XPNP_openPipe(const char* pipeName, int privatePipe);

int XPNP_writePipe(XPNP_PipeHandle pipeHandle, const char* pipeMsg, int bytesToWrite);

#ifdef __cplusplus
}
#endif
