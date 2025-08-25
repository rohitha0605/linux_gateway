#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool calc_handle_request(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_cap, size_t *out_len);
