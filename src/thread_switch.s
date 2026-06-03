.text
.global save_ctx
save_ctx:
    stp     x0, x1, [x0, #8]
    stp     x2, x3, [x0, #24]
    stp     x4, x5, [x0, #40]
    stp     x6, x7, [x0, #56]
    stp     x8, x9, [x0, #72]
    stp     x10, x11, [x0, #88]
    stp     x12, x13, [x0, #104]
    stp     x14, x15, [x0, #120]
    stp     x16, x17, [x0, #136]
    stp     x18, x19, [x0, #152]
    stp     x20, x21, [x0, #168]
    stp     x22, x23, [x0, #184]
    stp     x24, x25, [x0, #200]
    stp     x26, x27, [x0, #216]
    stp     x28, x29, [x0, #232]
    mov     x1, sp
    stp     x30, x1, [x0, #248]
    adr     x16, .
    str     x16, [x0, #264]
    ret

.global load_ctx
load_ctx:
    ldp     x30, x1, [x0, #248]
    add     sp, x1, #0
    ldp     x2, x3, [x0, #24]
    ldp     x4, x5, [x0, #40]
    ldp     x6, x7, [x0, #56]
    ldp     x8, x9, [x0, #72]
    ldp     x10, x11, [x0, #88]
    ldp     x12, x13, [x0, #104]
    ldp     x14, x15, [x0, #120]
    ldp     x16, x17, [x0, #136]
    ldp     x18, x19, [x0, #152]
    ldp     x20, x21, [x0, #168]
    ldp     x22, x23, [x0, #184]
    ldp     x24, x25, [x0, #200]
    ldp     x26, x27, [x0, #216]
    ldp     x28, x29, [x0, #232]
    ldr     x16, [x0, #264]
    ldp     x0, x1, [x0, #8]
    br      x16
    