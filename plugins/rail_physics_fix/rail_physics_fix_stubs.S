// GPL-3.0: adapted from RailPhysics 1.3.0 stubs.S.
// This is the readable source of , NOT an extra link input.
// tools/generate_stubs.py assembles it and resolves internal COFF relocations.
// All slots stay inside this blob; absolute helper/continuation addresses are
// populated while RW, then the entire block becomes RX before publishing hooks.
// No external symbols, absolute game addresses, or executable writable pages.
//
// Win64: reserve [rsp+0..31] for the callee, fifth argument at [rsp+32].
// Save registers ABOVE that area. The old source put saved GPRs in the callee's
// shadow space and passed the fifth float in xmm4 instead of on the stack.
// Preserve incoming flags as well; replayed cmp/comiss set the required flags.
.intel_syntax noprefix
.text

.macro SAVE
    pushfq
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 0xD0
    mov [rsp+0x30], rax
    mov [rsp+0x38], rcx
    mov [rsp+0x40], rdx
    mov [rsp+0x48], r8
    mov [rsp+0x50], r9
    mov [rsp+0x58], r10
    mov [rsp+0x60], r11
    movaps [rsp+0x70], xmm0
    movaps [rsp+0x80], xmm1
    movaps [rsp+0x90], xmm2
    movaps [rsp+0xA0], xmm3
    movaps [rsp+0xB0], xmm4
    movaps [rsp+0xC0], xmm5
.endm
.macro RESTORE
    mov rax, [rsp+0x30]
    mov rcx, [rsp+0x38]
    mov rdx, [rsp+0x40]
    mov r8,  [rsp+0x48]
    mov r9,  [rsp+0x50]
    mov r10, [rsp+0x58]
    mov r11, [rsp+0x60]
    movaps xmm1, [rsp+0x80]
    movaps xmm2, [rsp+0x90]
    movaps xmm3, [rsp+0xA0]
    movaps xmm4, [rsp+0xB0]
    movaps xmm5, [rsp+0xC0]
    mov rsp, rbp
    pop rbp
    popfq
.endm

.globl rp_brake_stub
rp_brake_stub:
    SAVE
    mov rcx, rsi
    movaps xmm1, xmm6
    movaps xmm2, xmm0
    movaps xmm3, xmm15
    movss [rsp+0x20], xmm9
    call qword ptr [rip+rp_brake_helper_ptr]
    RESTORE
    subss xmm6, xmm0
    movss dword ptr [rsi+0xd24], xmm6
    comiss xmm9, xmm6
    jmp qword ptr [rip+rp_brake_back]

.macro SLOPE name, back
.globl \name
\name:
    SAVE
    mov rcx, rsi
    movaps xmm1, xmm0
    movaps xmm2, xmm10
    movaps xmm3, xmm15
    call qword ptr [rip+rp_slope_helper_ptr]
    RESTORE
    addss xmm0, dword ptr [rsi+0xd24]
    movss dword ptr [rsi+0xd24], xmm0
    jmp qword ptr [rip+\back]
.endm
SLOPE rp_slope_stub, rp_slope_back
SLOPE rp_slope_stub2, rp_slope_back2

.globl rp_curve_stub
rp_curve_stub:
    SAVE
    mov rcx, rsi
    movaps xmm1, xmm9
    call qword ptr [rip+rp_curve_helper_ptr]
    movaps xmm9, xmm0
    movaps xmm0, [rsp+0x70]
    RESTORE
    cmp byte ptr [rsi+0xd41], r13b
    jz .Lcurve_branch
    movss xmm6, dword ptr [rsi+0xd24]
    jmp qword ptr [rip+rp_curve_back]
.Lcurve_branch:
    jmp qword ptr [rip+rp_curve_branch]

.globl rp_grid_stub
rp_grid_stub:
    mov rax, qword ptr [rbp+0x970]
    mulss xmm0, dword ptr [rdi+rbx+0x8c]
    mulss xmm0, dword ptr [rip+rp_grid_k]
    jmp qword ptr [rip+rp_grid_back]

.p2align 3
.macro SLOT name
.globl \name
\name:
    .quad 0
.endm
SLOT rp_brake_back
SLOT rp_slope_back
SLOT rp_slope_back2
SLOT rp_curve_back
SLOT rp_curve_branch
SLOT rp_grid_back
SLOT rp_brake_helper_ptr
SLOT rp_slope_helper_ptr
SLOT rp_curve_helper_ptr
.globl rp_grid_k
rp_grid_k:
    .float 1.0
