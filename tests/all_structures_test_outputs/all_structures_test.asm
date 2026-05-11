.data
__newline: .asciiz "\n"
.align 2
g_total_1: .space 4
g_i_0: .space 4
g_ch_2: .space 4
g_a_3: .space 20
g_p_4: .space 20
g_b_5: .space 28

.text
.globl main
.globl __start

__start:
  j main
  nop

main:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  li $t0, 1
  la $t1, g_i_0
  sw $t0, 0($t1)
  li $t0, 0
  la $t1, g_total_1
  sw $t0, 0($t1)
  li $t0, 65
  la $t1, g_ch_2
  sw $t0, 0($t1)
  addiu $sp, $sp, -12
  move $t0, $zero
  sw $t0, 0($sp)
  la $t0, g_a_3
  sw $t0, 4($sp)
  li $t0, 10
  sw $t0, 8($sp)
  jal proc_fill_6
  addiu $sp, $sp, 12
  addiu $sp, $sp, -20
  move $t0, $zero
  sw $t0, 0($sp)
  la $t0, g_p_4
  sw $t0, 4($sp)
  la $t0, g_b_5
  sw $t0, 8($sp)
  li $t0, 20
  sw $t0, 12($sp)
  la $t0, g_ch_2
  lw $t1, 0($t0)
  sw $t1, 16($sp)
  jal proc_touch_7
  addiu $sp, $sp, 20
  la $t0, g_a_3
  li $t1, 1
  addiu $t1, $t1, -1
  li $t2, 4
  mul $t1, $t1, $t2
  addu $t0, $t0, $t1
  lw $t1, 0($t0)
  la $t0, g_p_4
  addiu $t0, $t0, 8
  li $t2, 2
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  lw $t2, 0($t0)
  addu $t1, $t1, $t2
  la $t0, g_b_5
  addiu $t0, $t0, 4
  li $t2, 1
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  lw $t2, 0($t0)
  addu $t1, $t1, $t2
  la $t0, g_total_1
  sw $t1, 0($t0)
  la $t0, g_total_1
  lw $t1, 0($t0)
  li $t0, 100
  slt $t1, $t1, $t0
  beq $t1, $zero, if_else_0
  la $t0, g_total_1
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
  j if_end_1
if_else_0:
  la $t0, g_b_5
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
if_end_1:
  la $t0, g_p_4
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
  la $t0, g_p_4
  addiu $t0, $t0, 8
  li $t1, 3
  addiu $t1, $t1, -1
  li $t2, 4
  mul $t1, $t1, $t2
  addu $t0, $t0, $t1
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
  la $t0, g_b_5
  addiu $t0, $t0, 24
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  li $v0, 10
  syscall

proc_fill_6:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  addiu $sp, $sp, -4
  li $t0, 1
  move $t2, $fp
  addiu $t1, $t2, -4
  sw $t0, 0($t1)
while_begin_2:
  move $t1, $fp
  addiu $t0, $t1, -4
  lw $t1, 0($t0)
  li $t0, 6
  slt $t1, $t1, $t0
  beq $t1, $zero, while_end_3
  move $t1, $fp
  addiu $t0, $t1, 16
  lw $t1, 0($t0)
  move $t2, $fp
  addiu $t0, $t2, -4
  lw $t2, 0($t0)
  addu $t1, $t1, $t2
  move $t2, $fp
  lw $t0, 12($t2)
  move $t3, $fp
  addiu $t2, $t3, -4
  lw $t3, 0($t2)
  addiu $t3, $t3, -1
  li $t2, 4
  mul $t3, $t3, $t2
  addu $t0, $t0, $t3
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, -4
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  move $t2, $fp
  addiu $t0, $t2, -4
  sw $t1, 0($t0)
  j while_begin_2
while_end_3:
proc_fill_6_end:
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  jr $ra

proc_touch_7:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  move $t1, $fp
  addiu $t0, $t1, 20
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 12($t2)
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, 24
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 12($t2)
  addiu $t0, $t0, 4
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, 20
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 12($t2)
  addiu $t0, $t0, 8
  li $t2, 1
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, 20
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  move $t2, $fp
  lw $t0, 12($t2)
  addiu $t0, $t0, 8
  li $t2, 2
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, 20
  lw $t1, 0($t0)
  li $t0, 2
  addu $t1, $t1, $t0
  move $t2, $fp
  lw $t0, 12($t2)
  addiu $t0, $t0, 8
  li $t2, 3
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  sw $t1, 0($t0)
  move $t1, $fp
  lw $t0, 12($t1)
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 16($t2)
  sw $t1, 0($t0)
  move $t1, $fp
  lw $t0, 12($t1)
  addiu $t0, $t0, 8
  li $t1, 1
  addiu $t1, $t1, -1
  li $t2, 4
  mul $t1, $t1, $t2
  addu $t0, $t0, $t1
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 16($t2)
  addiu $t0, $t0, 4
  li $t2, 1
  addiu $t2, $t2, -1
  li $t3, 4
  mul $t2, $t2, $t3
  addu $t0, $t0, $t2
  sw $t1, 0($t0)
  move $t1, $fp
  lw $t0, 12($t1)
  addiu $t0, $t0, 4
  lw $t1, 0($t0)
  move $t2, $fp
  lw $t0, 16($t2)
  addiu $t0, $t0, 24
  sw $t1, 0($t0)
proc_touch_7_end:
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  jr $ra

