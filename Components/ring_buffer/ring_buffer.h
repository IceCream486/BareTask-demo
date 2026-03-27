/*
 * Modified Ring Buffer API (Decoupled from Zephyr)
 * Original Copyright (c) 2015 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Porting Adaptation Macros --- */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

/** Compiler optimization hint for branch prediction */
#ifndef unlikely
#define unlikely(x) (x)
#endif

/** * Index type for ring buffer. 
 * Using uint32_t allows buffers up to 2GB. 
 */
typedef uint32_t ring_buf_idx_t;

/** Internal index tracking structure */
struct ring_buf_index { 
    ring_buf_idx_t head, tail, base; 
};

/**
 * @brief Ring buffer instance structure
 */
struct ring_buf {
    uint8_t *buffer;           /**< Pointer to the underlying memory array */
    struct ring_buf_index put; /**< Write index tracking */
    struct ring_buf_index get; /**< Read index tracking */
    uint32_t size;             /**< Buffer capacity in bytes */
};

/* --- Internal Functions --- */
uint32_t ring_buf_area_claim(struct ring_buf *buf, struct ring_buf_index *ring,
                             uint8_t **data, uint32_t size);
int ring_buf_area_finish(struct ring_buf *buf, struct ring_buf_index *ring,
                         uint32_t size);

/* --- Initialization Macros --- */

/**
 * @brief Helper macro to initialize a ring_buf structure
 */
#define RING_BUF_INIT(_buf, _size) { \
    .buffer = (_buf),                \
    .size = (_size),                 \
}

/**
 * @brief Statically define and initialize a ring buffer
 * @param name Variable name
 * @param size8 Capacity in bytes
 */
#define RING_BUF_DECLARE(name, size8) \
    static uint8_t _ring_buffer_data_##name[size8]; \
    struct ring_buf name = RING_BUF_INIT(_ring_buffer_data_##name, size8)

/* --- Management API --- */

/**
 * @brief Initialize a ring buffer at runtime
 * @param buf Pointer to ring buffer instance
 * @param size Capacity in bytes
 * @param data Pointer to the memory array to be used as storage
 */
static inline void ring_buf_init(struct ring_buf *buf, uint32_t size, uint8_t *data)
{
    buf->size = size;
    buf->buffer = data;
    buf->put.head = buf->put.tail = buf->put.base = 0;
    buf->get.head = buf->get.tail = buf->get.base = 0;
}

/** @brief Check if buffer is empty */
static inline bool ring_buf_is_empty(const struct ring_buf *buf)
{
    return buf->get.head == buf->put.tail;
}

/** @brief Reset buffer to empty state */
static inline void ring_buf_reset(struct ring_buf *buf)
{
    buf->put.head = buf->put.tail = buf->put.base = 0;
    buf->get.head = buf->get.tail = buf->get.base = 0;
}

/** @brief Get remaining free space in bytes */
static inline uint32_t ring_buf_space_get(const struct ring_buf *buf)
{
    return buf->size - (buf->put.head - buf->get.tail);
}

/** @brief Get total buffer capacity in bytes */
static inline uint32_t ring_buf_capacity_get(const struct ring_buf *buf)
{
    return buf->size;
}

/** @brief Get number of bytes currently stored in buffer */
static inline uint32_t ring_buf_size_get(const struct ring_buf *buf)
{
    return buf->put.tail - buf->get.head;
}

/* --- Byte-oriented API --- */

/**
 * @brief Claim contiguous free space for writing
 * @param data Output pointer to the writeable area
 * @return Number of contiguous bytes available
 */
static inline uint32_t ring_buf_put_claim(struct ring_buf *buf, uint8_t **data, uint32_t size)
{
    uint32_t space = ring_buf_space_get(buf);
    return ring_buf_area_claim(buf, &buf->put, data, MIN(size, space));
}

/**
 * @brief Indicate that writing to the claimed area is finished
 * @return 0 on success, negative error code otherwise
 */
static inline int ring_buf_put_finish(struct ring_buf *buf, uint32_t size)
{
    return ring_buf_area_finish(buf, &buf->put, size);
}

/** @brief Put data into the buffer. Returns bytes actually written. */
uint32_t ring_buf_put(struct ring_buf *buf, const uint8_t *data, uint32_t size);

/**
 * @brief Claim contiguous stored data for reading
 * @param data Output pointer to the readable data
 * @return Number of contiguous bytes available
 */
static inline uint32_t ring_buf_get_claim(struct ring_buf *buf, uint8_t **data, uint32_t size)
{
    uint32_t buf_size = ring_buf_size_get(buf);
    return ring_buf_area_claim(buf, &buf->get, data, MIN(size, buf_size));
}

/**
 * @brief Indicate that reading from the claimed area is finished
 * @return 0 on success, negative error code otherwise
 */
static inline int ring_buf_get_finish(struct ring_buf *buf, uint32_t size)
{
    return ring_buf_area_finish(buf, &buf->get, size);
}

/** @brief Get data from the buffer. Returns bytes actually read. */
uint32_t ring_buf_get(struct ring_buf *buf, uint8_t *data, uint32_t size);

/** @brief Peek data without removing it from buffer. Returns bytes copied. */
uint32_t ring_buf_peek(struct ring_buf *buf, uint8_t *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H_ */