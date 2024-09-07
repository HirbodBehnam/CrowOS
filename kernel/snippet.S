#include "gdt.h"

.section .text
.intel_syntax noprefix
.global reload_segments
.type reload_segments, @function
reload_segments:
    LGDT [RDI]
    PUSH GDT_CODE_SEGMENT
    LEA RAX, [.reload_CS]
    PUSH RAX
    RETFQ
.reload_CS:
    MOV   AX, GDT_DATA_SEGMENT
    MOV   DS, AX
    MOV   ES, AX
    MOV   FS, AX
    MOV   GS, AX
    MOV   SS, AX
    RET
