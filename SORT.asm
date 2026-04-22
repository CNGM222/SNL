.data
__newline: .asciiz "\n"
.align 2
g_j_1: .space 4
g_a_3: .space 80
g_i_0: .space 4
g_num_2: .space 4

.text
.globl main

main:
  move $fp, $sp
  la $t0, g_num_2
  li $v0, 5
  syscall
  sw $v0, 0($t0)
  li $t0, 1
  la $t1, g_i_0
  sw $t0, 0($t1)
while_begin_0:
  la $t0, g_i_0
  lw $t1, 0($t0)
  la $t0, g_num_2
  lw $t2, 0($t0)
  li $t0, 1
  addu $t2, $t2, $t0
  slt $t1, $t1, $t2
  beq $t1, $zero, while_end_1
  la $t0, g_j_1
  li $v0, 5
  syscall
  sw $v0, 0($t0)
  la $t0, g_j_1
  lw $t1, 0($t0)
  la $t0, g_a_3
  la $t2, g_i_0
  lw $t3, 0($t2)
  addiu $t3, $t3, -1
  li $t2, 4
  mul $t3, $t3, $t2
  addu $t0, $t0, $t3
  sw $t1, 0($t0)
  la $t0, g_i_0
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  la $t0, g_i_0
  sw $t1, 0($t0)
  j while_begin_0
while_end_1:
  la $t0, g_num_2
  lw $t1, 0($t0)
  addiu $sp, $sp, -4
  sw $t1, 0($sp)
  move $t0, $zero
  addiu $sp, $sp, -4
  sw $t0, 0($sp)
  jal proc_q_4
  addiu $sp, $sp, 8
  li $t0, 1
  la $t1, g_i_0
  sw $t0, 0($t1)
while_begin_2:
  la $t0, g_i_0
  lw $t1, 0($t0)
  la $t0, g_num_2
  lw $t2, 0($t0)
  li $t0, 1
  addu $t2, $t2, $t0
  slt $t1, $t1, $t2
  beq $t1, $zero, while_end_3
  la $t0, g_a_3
  la $t1, g_i_0
  lw $t2, 0($t1)
  addiu $t2, $t2, -1
  li $t1, 4
  mul $t2, $t2, $t1
  addu $t0, $t0, $t2
  lw $t1, 0($t0)
  move $a0, $t1
  li $v0, 1
  syscall
  la $a0, __newline
  li $v0, 4
  syscall
  la $t0, g_i_0
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  la $t0, g_i_0
  sw $t1, 0($t0)
  j while_begin_2
while_end_3:
  li $v0, 10
  syscall

proc_q_4:
  addiu $sp, $sp, -8
  sw $fp, 0($sp)
  sw $ra, 4($sp)
  move $fp, $sp
  addiu $sp, $sp, -16
  li $t0, 1
  move $t2, $fp
  addiu $t1, $t2, -4
  sw $t0, 0($t1)
  li $t0, 1
  move $t2, $fp
  addiu $t1, $t2, -8
  sw $t0, 0($t1)
while_begin_4:
  move $t1, $fp
  addiu $t0, $t1, -4
  lw $t1, 0($t0)
  move $t2, $fp
  addiu $t0, $t2, 12
  lw $t2, 0($t0)
  slt $t1, $t1, $t2
  beq $t1, $zero, while_end_5
  move $t1, $fp
  addiu $t0, $t1, 12
  lw $t1, 0($t0)
  move $t2, $fp
  addiu $t0, $t2, -4
  lw $t2, 0($t0)
  subu $t1, $t1, $t2
  li $t0, 1
  addu $t1, $t1, $t0
  move $t2, $fp
  addiu $t0, $t2, -8
  sw $t1, 0($t0)
  li $t0, 1
  move $t2, $fp
  addiu $t1, $t2, -12
  sw $t0, 0($t1)
while_begin_6:
  move $t1, $fp
  addiu $t0, $t1, -12
  lw $t1, 0($t0)
  move $t2, $fp
  addiu $t0, $t2, -8
  lw $t2, 0($t0)
  slt $t1, $t1, $t2
  beq $t1, $zero, while_end_7
  la $t0, g_a_3
  move $t2, $fp
  addiu $t1, $t2, -12
  lw $t2, 0($t1)
  li $t1, 1
  addu $t2, $t2, $t1
  addiu $t2, $t2, -1
  li $t1, 4
  mul $t2, $t2, $t1
  addu $t0, $t0, $t2
  lw $t1, 0($t0)
  la $t0, g_a_3
  move $t3, $fp
  addiu $t2, $t3, -12
  lw $t3, 0($t2)
  addiu $t3, $t3, -1
  li $t2, 4
  mul $t3, $t3, $t2
  addu $t0, $t0, $t3
  lw $t2, 0($t0)
  slt $t1, $t1, $t2
  beq $t1, $zero, if_else_8
  la $t0, g_a_3
  move $t2, $fp
  addiu $t1, $t2, -12
  lw $t2, 0($t1)
  addiu $t2, $t2, -1
  li $t1, 4
  mul $t2, $t2, $t1
  addu $t0, $t0, $t2
  lw $t1, 0($t0)
  move $t2, $fp
  addiu $t0, $t2, -16
  sw $t1, 0($t0)
  la $t0, g_a_3
  move $t2, $fp
  addiu $t1, $t2, -12
  lw $t2, 0($t1)
  li $t1, 1
  addu $t2, $t2, $t1
  addiu $t2, $t2, -1
  li $t1, 4
  mul $t2, $t2, $t1
  addu $t0, $t0, $t2
  lw $t1, 0($t0)
  la $t0, g_a_3
  move $t3, $fp
  addiu $t2, $t3, -12
  lw $t3, 0($t2)
  addiu $t3, $t3, -1
  li $t2, 4
  mul $t3, $t3, $t2
  addu $t0, $t0, $t3
  sw $t1, 0($t0)
  move $t1, $fp
  addiu $t0, $t1, -16
  lw $t1, 0($t0)
  la $t0, g_a_3
  move $t3, $fp
  addiu $t2, $t3, -12
  lw $t3, 0($t2)
  li $t2, 1
  addu $t3, $t3, $t2
  addiu $t3, $t3, -1
  li $t2, 4
  mul $t3, $t3, $t2
  addu $t0, $t0, $t3
  sw $t1, 0($t0)
  j if_end_9
if_else_8:
  li $t0, 0
  move $t2, $fp
  addiu $t1, $t2, -16
  sw $t0, 0($t1)
if_end_9:
  move $t1, $fp
  addiu $t0, $t1, -12
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  move $t2, $fp
  addiu $t0, $t2, -12
  sw $t1, 0($t0)
  j while_begin_6
while_end_7:
  move $t1, $fp
  addiu $t0, $t1, -4
  lw $t1, 0($t0)
  li $t0, 1
  addu $t1, $t1, $t0
  move $t2, $fp
  addiu $t0, $t2, -4
  sw $t1, 0($t0)
  j while_begin_4
while_end_5:
proc_q_4_end:
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addiu $sp, $sp, 8
  jr $ra

