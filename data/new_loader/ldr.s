# 
# This is free and unencumbered software released into the public domain.
# 
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
# 
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
.global ldr
.global ldr_END

.global memldr
.global memldr_END

#constants
.set SIO_BASE_ADDR, 	0x1F801050
.set SIO_TX_RX_OSET,	0x0
.set SIO_STAT_OSET, 	0x4
.set SIO_CTRL_OSET, 	0xA

.set SIO_TX_RX_ADDR, 	0x1F801050
.set SIO_STAT_ADDR,  	0x1F801054
.set SIO_CTRL_ADDR, 	0x1F80105A
.set SIO_CTRL_CTS,   	0x00000020
.set SIO_STATR_RXRDY, 	0x00000002

ldr: # a0 = address of PSX-EXE buffer
	lw $t0, 0x0($a0) # Load the write addres (pc0)
	lw $t1, 0xc($a0) # Load text section size (t_size)
	#lw $sp, 0x20($a0) # Load initial sp value (s_addr) ~ Avoid doing this, don't think it's needed.
	li $a1, SIO_BASE_ADDR # Load serial io read/write address
	li $a2, SIO_STAT_ADDR # Load serial io status reg address
	li $a3, SIO_CTRL_ADDR # Load serial io ctrl reg address
	li $t4, -1
	li $t5, SIO_CTRL_CTS 
	xor $t5, $t4, $t5 # Store ~CTS

ldr_readSIO:
	lh $t3, ($a3)	# read sio ctrl reg
	ori $t3, $t3, SIO_CTRL_CTS  # CTS: on
	sh $t3, ($a3)	# write CTS: on
ldr_sioWait:
	lw $t3, ($a2)		# read sio status reg
	andi $t3, $t3, SIO_STATR_RXRDY 	# check RX has data in
	beq $t3, $zero, ldr_sioWait 	# wait on RX flag to be on
	nop

	lb $t3, ($a1)		# read the sio data
	sb $t3, 0($t0)				# write to write address

	lh $t3, ($a3)	# read sio ctrl reg
	and $t3, $t5, $t3 			# CTS: off
	sh $t3, ($a3)	# write CTS: off

	addiu $t0, 1			# pc0 + 1
	addiu $t1, -1			# t_size - 1
	bne $t1, $zero, ldr_readSIO
	nop

	# Custom version
	#lw $t0, ($a0) # Load the write addres (pc0)
	#jr $t0 # jump to program start
	#nop
	# Using syscalls. Can't use syscall.s version, may have been destroyed
	# call EnterCriticalSection
	or $t5, $a0, $a0 # $t5=$a0 
	li $a0, 1
	syscall
	nop
	# call Exec
	or $a0, $t5, $t5 # $a0=$t5 
	li $a1, 1 # argc
	li $a2, 0 # argv (null)
	li $9, 0x43 
	j 0xa0 # call Exec. 
	nop

ldr_END:

memldr:
	lw $t0, 0x0($a0) # Load the write addres (pc0)
	lw $t1, 0xc($a0) # Load text section size (t_size)
	lw $t2, ($a1) # source address

memldr_copyloop:
	lb $t3, ($t2)
	sb $t3, ($t0)

	addiu $t0, 1
	addiu $t2, 1
	addiu $t1, -1
	bne $t1, $zero, memldr_copyloop
	nop

	# Custom version
	#lw $t0, ($a0) # Load the write addres (pc0)
	#jr $t0 # jump to program start
	#nop
	# Using syscalls. Can't use syscall.s version, may have been destroyed
	# call EnterCriticalSection
	or $t5, $a0, $a0 # $t5=$a0 
	li $a0, 1
	syscall
	nop
	# call Exec
	or $a0, $t5, $t5 # $a0=$t5 
	li $a1, 1 # argc
	li $a2, 0 # argv (null)
	li $9, 0x43 
	j 0xa0 # call Exec. 
	nop
memldr_END: