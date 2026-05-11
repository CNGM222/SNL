.data
__newline: .asciiz "\n"
.align 2
g_n_0: .space 4
g_oddflag_1: .space 4

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
  li $t0, 7
  la $t1, g_n_0
  sw $t0, 0($t1)
  addiu $sp, $sp, -12
  move $t0, $zero
  sw $t0, 0($sp)
  la $t0, g_n_0
  lw $t1, 0($t0)
  sw $t1, 4($sp)
  la $t0, g_oddflag_1
  sw $t0, 8($sp)
  jal proc_odd_2
  addiu $sp, $sp, 12
  la $t0, g_oddflag_1
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

proc_odd_2:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  move $t1, $fp
  addiu $t0, $t1, 12
  lw $t1, 0($t0)
  li $t0, 0
  xor $t1, $t1, $t0
  sltiu $t1, $t1, 1
  beq $t1, $zero, if_else_0
  li $t0, 0
  move $t2, $fp
  lw $t1, 16($t2)
  sw $t0, 0($t1)
  j if_end_1
if_else_0:
  addiu $sp, $sp, -12
  move $t0, $zero
  sw $t0, 0($sp)
  move $t1, $fp
  addiu $t0, $t1, 12
  lw $t1, 0($t0)
  li $t0, 1
  subu $t1, $t1, $t0
  sw $t1, 4($sp)
  move $t1, $fp
  lw $t0, 16($t1)
  sw $t0, 8($sp)
  jal proc_even_3
  addiu $sp, $sp, 12
if_end_1:
proc_odd_2_end:
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  jr $ra

proc_even_3:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  move $t1, $fp
  addiu $t0, $t1, 12
  lw $t1, 0($t0)
  li $t0, 0
  xor $t1, $t1, $t0
  sltiu $t1, $t1, 1
  beq $t1, $zero, if_else_2
  li $t0, 1
  move $t2, $fp
  lw $t1, 16($t2)
  sw $t0, 0($t1)
  j if_end_3
if_else_2:
  addiu $sp, $sp, -12
  move $t0, $zero
  sw $t0, 0($sp)
  move $t1, $fp
  addiu $t0, $t1, 12
  lw $t1, 0($t0)
  li $t0, 1
  subu $t1, $t1, $t0
  sw $t1, 4($sp)
  move $t1, $fp
  lw $t0, 16($t1)
  sw $t0, 8($sp)
  jal proc_odd_2
  addiu $sp, $sp, 12
if_end_3:
proc_even_3_end:
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  jr $ra

