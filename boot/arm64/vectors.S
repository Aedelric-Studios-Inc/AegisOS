/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/vectors.S
 * AArch64 exception vector table.
 */

.macro vector_entry label
    .align 7
    b \label
.endm

.section ".text.vectors", "ax"
.global _vectors
.align 11
_vectors:
    /* Current EL with SP0 */
    vector_entry el1_sync_sp0
    vector_entry el1_irq_sp0
    vector_entry el1_fiq_sp0
    vector_entry el1_error_sp0
    /* Current EL with SPx */
    vector_entry el1_sync_spx
    vector_entry el1_irq_spx
    vector_entry el1_fiq_spx
    vector_entry el1_error_spx
    /* Lower EL using AArch64 */
    vector_entry el0_sync_a64
    vector_entry el0_irq_a64
    vector_entry el0_fiq_a64
    vector_entry el0_error_a64
    /* Lower EL using AArch32 */
    vector_entry el0_sync_a32
    vector_entry el0_irq_a32
    vector_entry el0_fiq_a32
    vector_entry el0_error_a32
