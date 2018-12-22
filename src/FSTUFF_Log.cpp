//
//  FSTUFF_Log.cpp
//  FallingStuff
//
//  Created by David Ludwig on 11/20/18.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"

#if __has_include(<windows.h>)
    #include <windows.h>
    #define FSTUFF_USE_OUTPUTDEBUGSTRING 1
#endif

void FSTUFF_Log(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[2048];
    vsnprintf(buffer, FSTUFF_countof(buffer), fmt, args);
	printf("%s", buffer);
#if FSTUFF_USE_OUTPUTDEBUGSTRING
	OutputDebugStringA(buffer);
#endif
    va_end(args);
}
