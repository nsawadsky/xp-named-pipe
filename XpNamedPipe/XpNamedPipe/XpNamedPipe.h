#pragma once

#include <string>
#include <vector>

typedef void* XPNP_PipeHandle;

std::string XPNP_getErrorMessage();

bool XPNP_makePipeName(const std::string& baseName, bool userLocal, std::string& fullPipeName);
 
XPNP_PipeHandle XPNP_createPipe(const std::string& pipeName, bool privatePipe);

bool XPNP_stopPipe(XPNP_PipeHandle pipeHandle);

bool XPNP_deletePipe(XPNP_PipeHandle pipeHandle);

XPNP_PipeHandle XPNP_acceptConnection(XPNP_PipeHandle pipeHandle);

bool XPNP_readMessage(XPNP_PipeHandle pipeHandle, std::vector<char>& buffer);

XPNP_PipeHandle XPNP_openPipe(const std::string& pipeName, bool privatePipe);

bool XPNP_writeMessage(XPNP_PipeHandle pipeHandle, char* pipeMsg, int bytesToWrite);
