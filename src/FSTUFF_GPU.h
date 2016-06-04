//
//  FSTUFF_GPU.h
//  FallingStuff
//
//  Created by David Ludwig on 6/3/16.
//  Copyright © 2016 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_GPU_h
#define FSTUFF_GPU_h

#include <stddef.h>

void * FSTUFF_VertexBufferNew(void * gpuDevice, void * src, size_t size);
void FSTUFF_VertexBufferDestroy(void * gpuVertexBuffer);

#endif /* FSTUFF_GPU_h */
