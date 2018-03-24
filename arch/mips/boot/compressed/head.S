/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */

#include <asm/asm.h>
#include <asm/regdef.h>

	.set noreorder
	.cprestore
	LEAF(start)
start:
	/* Save boot rom start args */
	move	s0, a0
	move	s1, a1
	move	s2, a2
	move	s3, a3

	/* Get the offset between the current address and linked address */
	PTR_LA	t0, reloc_label
	bal	reloc_label
	 nop
reloc_label:
	subu	t0, ra, t0

	/* If there is no offset no reloc is needed */
	beqz	t0, clear_bss
	 nop

	/* Move the text, data section and DTB to the correct address */
	PTR_LA	a0, .text
	addu	a1, t0, a0
	PTR_LA	a2, _edata
copy_vmlinuz:
	lw	a3, 0(a1)
	sw	a3, 0(a0)
	addiu	a0, a0, 4
	bne	a2, a0, copy_vmlinuz
	 addiu	a1, a1, 4

clear_bss:
	/* Clear BSS */
	PTR_LA	a0, _edata
	PTR_LA	a2, _end
1:	sw	zero, 0(a0)
	bne	a2, a0, 1b
	 addiu	a0, a0, 4

	PTR_LA	a0, (.heap)	     /* heap address */
	PTR_LA	sp, (.stack + 8192)  /* stack address */

	PTR_LA	ra, 2f
	PTR_LA	k0, decompress_kernel
	jr	k0
	 nop
2:
	move	a0, s0
	move	a1, s1
	move	a2, s2
	move	a3, s3
	PTR_LI	k0, KERNEL_ENTRY
	jr	k0
	 nop
3:
	b	3b
	 nop
	END(start)

	.comm .heap,BOOT_HEAP_SIZE,4
	.comm .stack,4096*2,4
