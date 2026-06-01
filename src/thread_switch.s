.text
.global save_ctx
save_ctx:
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x8, [x0, #80]
    mov     x1, sp
    stp     x30, x1, [x0, #96]
    ret

.global load_ctx
load_ctx:
    ldp     x19, x20, [x0, #0]
    ldp     x21, x22, [x0, #16]
    ldp     x23, x24, [x0, #32]
    ldp     x25, x26, [x0, #48]
    ldp     x27, x28, [x0, #64]
    ldp     x29, x8, [x0, #80]
    ldp     x30, x1, [x0, #96]
    add     sp, x1, #0
    ret

.global _start
_start:
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x8, [x0, #80]
    mov     x2, sp
    stp     x30, x2, [x0, #96]

    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x8, [x1, #80]
    ldp     x30, x2, [x1, #96]
    add     sp, x2, #0
    ret
    