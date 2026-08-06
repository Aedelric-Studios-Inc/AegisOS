/* SPDX-License-Identifier: Proprietary */
#pragma once
/* Exception/vector bring-up helpers. */

#include "types.h"

typedef struct aegis_exception_fault {
    u64 esr;
    u64 far;
    u64 elr;
    u64 ec;
    u64 iss;
    u64 fsc;
    bool write;
    bool instruction_abort;
    bool data_abort;
    bool from_lower_el;
    bool from_current_el;
    bool is_page_fault;
} aegis_exception_fault_t;

u64 exception_vector_base_read(void);
u64 exception_vector_table_addr(void);
int exception_vector_selftest(void);
int exception_decode_fault(u64 esr, u64 far, u64 elr, aegis_exception_fault_t *out);
bool exception_fault_is_user_page_fault(const aegis_exception_fault_t *fault);
