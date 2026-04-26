/*
 * MIT License
 *
 * Copyright (c) 2024 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SANO_RTOS_MEM_SLAB_H
#define __SANO_RTOS_MEM_SLAB_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a memory slab.
 *
 * Each slab provides a fixed number of fixed-size blocks. Block size must be
 * pointer aligned because free blocks store the next free pointer in-place.
 *
 * @param _name Name of the slab.
 * @param _blockSize Size of each block in bytes.
 * @param _numBlocks Number of blocks in the slab.
 */
#define MEM_SLAB_DEFINE(_name, _blockSize, _numBlocks)                                                           \
    _Static_assert(((_blockSize) >= sizeof(void *)), "Memory slab block size must fit a pointer.");            \
    _Static_assert(((_blockSize) % sizeof(void *)) == 0U, "Memory slab block size must be pointer aligned.");  \
    _Static_assert(((_numBlocks) > 0U), "Memory slab must contain at least one block.");                       \
    uint8_t _name##Buffer[(_blockSize) * (_numBlocks)] __attribute__((aligned(sizeof(void *))));               \
    memSlabHandleType _name = {                                                                                 \
        .name = #_name,                                                                                         \
        .lock = 0,                                                                                              \
        .waitQueue = {0},                                                                                       \
        .buffer = _name##Buffer,                                                                                \
        .blockSize = (_blockSize),                                                                              \
        .numBlocks = (_numBlocks),                                                                              \
        .freeBlocks = (_numBlocks),                                                                             \
        .freeList = NULL,                                                                                       \
        .initialized = false}

    /**
     * @brief Fixed-size memory slab object.
     */
    typedef struct
    {
        const char *name;        ///< Name of the slab.
        atomic_t lock;           ///< Spinlock protecting slab state.
        taskQueueType waitQueue; ///< Tasks waiting for a free block.
        uint8_t *buffer;         ///< Backing storage for all blocks.
        uint32_t blockSize;      ///< Size of each block in bytes.
        uint32_t numBlocks;      ///< Total number of blocks.
        uint32_t freeBlocks;     ///< Number of currently free blocks.
        void *freeList;          ///< Head of the in-place free list.
        bool initialized;        ///< Lazy initialization flag for static slabs.
    } memSlabHandleType;

    /**
     * @brief Get the number of free blocks currently available.
     *
     * @param pMemSlab Pointer to memory slab handle.
     * @return Number of free blocks, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t memSlabFreeBlocks(memSlabHandleType *pMemSlab)
    {
        return (pMemSlab == NULL) ? 0U : pMemSlab->freeBlocks;
    }

    /**
     * @brief Get the block size of a memory slab.
     *
     * @param pMemSlab Pointer to memory slab handle.
     * @return Block size in bytes, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t memSlabBlockSize(memSlabHandleType *pMemSlab)
    {
        return (pMemSlab == NULL) ? 0U : pMemSlab->blockSize;
    }

    /**
     * @brief Allocate one block from a memory slab.
     *
     * If no free block is available, the caller may wait up to `waitTicks`.
     * If called from ISR, `waitTicks` must be `TASK_NO_WAIT`.
     *
     * @param pMemSlab Pointer to memory slab handle.
     * @param ppBlock Updated with the allocated block pointer on success.
     * @param waitTicks Maximum ticks to wait for a free block.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if no free block is available and no wait was requested,
     *         `RET_TIMEOUT` on timeout, `RET_INVAL` for invalid arguments or ISR wait usage, or another error code.
     */
    int memSlabAlloc(memSlabHandleType *pMemSlab, void **ppBlock, uint32_t waitTicks);

    /**
     * @brief Free one block back to a memory slab.
     *
     * @param pMemSlab Pointer to memory slab handle.
     * @param pBlock Pointer to the block to free.
     * @return `RET_SUCCESS` on success, `RET_INVAL` for invalid arguments or block pointer, or another error code.
     */
    int memSlabFree(memSlabHandleType *pMemSlab, void *pBlock);

#ifdef __cplusplus
}
#endif

#endif
