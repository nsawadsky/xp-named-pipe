// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <WinSock2.h>
#include <sddl.h>
#include <Rpc.h>

// Avoid warnings for use of throw as exception specification.
#pragma warning( disable : 4290 )
