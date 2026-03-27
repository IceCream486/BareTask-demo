/*
 * Modified Ring Buffer Implementation
 * Original Copyright (c) 2015 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ring_buffer.h"
#include <string.h>

/**
 * @brief Internal function to claim a contiguous block of memory from the buffer.
 *
 * This function calculates the available contiguous memory from the current 
 * head position until the end of the buffer (to handle wrap-around).
 *
 * @param buf  Pointer to the ring buffer.
 * @param ring Pointer to the specific index tracker (put or get).
 * @param data Output pointer to the start of the claimed memory block.
 * @param size Requested size to claim.
 *
 * @return Actual size of the contiguous block claimed.
 */
uint32_t ring_buf_area_claim(struct ring_buf *buf, struct ring_buf_index *ring,
			     uint8_t **data, uint32_t size)
{
	ring_buf_idx_t head_offset, wrap_size;

	/* Calculate the logical offset from the current base */
	head_offset = ring->head - ring->base;
	if (unlikely(head_offset >= buf->size)) {
		head_offset -= buf->size;
	}

	/* Check how much space is left before the physical end of the array */
	wrap_size = buf->size - head_offset;
	size = MIN(size, wrap_size);

	/* Provide the physical address in the buffer */
	*data = &buf->buffer[head_offset];
	ring->head += size;

	return size;
}

/**
 * @brief Internal function to finalize a previously claimed memory operation.
 *
 * Updates the tail and base pointers to reflect that the data has been 
 * effectively written to or read from the buffer.
 *
 * @param buf  Pointer to the ring buffer.
 * @param ring Pointer to the specific index tracker (put or get).
 * @param size Actual size of data processed.
 *
 * @return 0 on success, negative errno on failure.
 */
int ring_buf_area_finish(struct ring_buf *buf, struct ring_buf_index *ring,
			 uint32_t size)
{
	ring_buf_idx_t claimed_size, tail_offset;

	/* Ensure the finished size does not exceed what was claimed */
	claimed_size = ring->head - ring->tail;
	if (unlikely(size > claimed_size)) {
		return -EINVAL;
	}

	/* Move the tail to 'catch up' with the processed head */
	ring->tail += size;
	ring->head = ring->tail;

	/* If the tail passed the buffer boundary, shift the logical base */
	tail_offset = ring->tail - ring->base;
	if (unlikely(tail_offset >= buf->size)) {
		ring->base += buf->size;
	}

	return 0;
}

/**
 * @brief Copy data into the ring buffer.
 *
 * This function copies a specified amount of data into the buffer. 
 * It handles internal wrap-around automatically by performing multiple 
 * contiguous copies if necessary.
 *
 * @param buf  Pointer to the ring buffer.
 * @param data Source data address.
 * @param size Number of bytes to write.
 *
 * @return Number of bytes actually written.
 */
uint32_t ring_buf_put(struct ring_buf *buf, const uint8_t *data, uint32_t size)
{
	uint8_t *dst;
	uint32_t partial_size;
	uint32_t total_size = 0;

	/* Loop until all data is written or buffer is full */
	do {
		/* Claim a contiguous chunk */
		partial_size = ring_buf_put_claim(buf, &dst, size);
		if (partial_size == 0) {
			break;
		}
		/* Copy data to the claimed memory */
		memcpy(dst, data, partial_size);
		total_size += partial_size;
		size -= partial_size;
		data += partial_size;
	} while (size != 0);

	/* Confirm the total bytes written */
	ring_buf_put_finish(buf, total_size);

	return total_size;
}

/**
 * @brief Retrieve data from the ring buffer.
 *
 * This function copies data from the buffer to a destination array. 
 * If the destination pointer is NULL, data is simply discarded (dropped).
 *
 * @param buf  Pointer to the ring buffer.
 * @param data Destination data address (can be NULL).
 * @param size Number of bytes to read.
 *
 * @return Number of bytes actually read.
 */
uint32_t ring_buf_get(struct ring_buf *buf, uint8_t *data, uint32_t size)
{
	uint8_t *src;
	uint32_t partial_size;
	uint32_t total_size = 0;

	/* Loop until requested size is met or buffer is empty */
	do {
		/* Claim a contiguous chunk for reading */
		partial_size = ring_buf_get_claim(buf, &src, size);
		if (partial_size == 0) {
			break;
		}
		/* Copy to output if pointer is provided */
		if (data) {
			memcpy(data, src, partial_size);
			data += partial_size;
		}
		total_size += partial_size;
		size -= partial_size;
	} while (size != 0);

	/* Confirm the total bytes read (moving the pointers) */
	ring_buf_get_finish(buf, total_size);

	return total_size;
}

/**
 * @brief Read data from the ring buffer without removing it.
 *
 * Similar to ring_buf_get, but it does not move the internal read pointers.
 * The next call to get or peek will yield the same data.
 *
 * @param buf  Pointer to the ring buffer.
 * @param data Destination data address (can be NULL).
 * @param size Number of bytes to peek.
 *
 * @return Number of bytes actually copied.
 */
uint32_t ring_buf_peek(struct ring_buf *buf, uint8_t *data, uint32_t size)
{
	uint8_t *src;
	uint32_t partial_size;
	uint32_t total_size = 0;

	/* Simulate reading process */
	do {
		partial_size = ring_buf_get_claim(buf, &src, size);
		if (partial_size == 0) {
			break;
		}
		if (data != NULL) {
			memcpy(data, src, partial_size);
			data += partial_size;
		}
		total_size += partial_size;
		size -= partial_size;
	} while (size != 0);

	/* Finalize with 0 to reset the temporary 'head' without moving 'tail' */
	ring_buf_get_finish(buf, 0);

	return total_size;
}