.text
.global thread_switch
thread_switch:
    stp     x29, x30, [sp, #-16]!
    
    ldp     x29, x30, [sp], #16
    ret