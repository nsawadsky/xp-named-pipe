#pragma once

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* XPNP_PipeHandle;

void XPNP_getErrorMessage(char* buffer, int bufLen);

int XPNP_makePipeName(const char* baseName, bool userLocal, char* pipeNameBuf, int bufLen);
 
XPNP_PipeHandle XPNP_createPipe(const char* pipeName, bool privatePipe);

int XPNP_stopPipe(XPNP_PipeHandle pipeHandle);

int XPNP_deletePipe(XPNP_PipeHandle pipeHandle);

XPNP_PipeHandle XPNP_acceptConnection(XPNP_PipeHandle pipeHandle);

int XPNP_readMessage(XPNP_PipeHandle pipeHandle, char* buffer, int bufLen, int* bytesRemaining);

XPNP_PipeHandle XPNP_openPipe(const char* pipeName, bool privatePipe);

int XPNP_writeMessage(XPNP_PipeHandle pipeHandle, const char* pipeMsg, int bytesToWrite);

#ifdef __cplusplus
}
#endif
