/*
    Copyright � 1995-2010, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Code to dynamically load ELF executables
    Lang: english

    1997/12/13: Changed filename to internalloadseg_elf.c
                Original file was created by digulla.
*/

#define DEBUG 0

#include <aros/config.h>
#include <aros/kernel.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <dos/elf.h>
#include <dos/dosasl.h>
#include <proto/dos.h>
#include <proto/arossupport.h>
#include <proto/kernel.h>
#include <aros/asmcall.h>
#include "internalloadseg.h"
#include "dos_intern.h"

#include <aros/debug.h>
#include <string.h>
#include <stddef.h>

#include <aros/macros.h>

#if (__WORDSIZE == 64)
#define WANT_CLASS ELFCLASS64
#else
#define WANT_CLASS ELFCLASS32
#endif

#if AROS_BIG_ENDIAN
#define WANT_BYTE_ORDER ELFDATA2MSB
#else
#define WANT_BYTE_ORDER ELFDATA2LSB
#endif

struct hunk
{
    ULONG size;
    BPTR  next;
    char  data[0];
} __attribute__((packed));

#define BPTR2HUNK(bptr) ((struct hunk *)((void *)bptr - offsetof(struct hunk, next)))
#define HUNK2BPTR(hunk) MKBADDR(&hunk->next)

#undef MyRead
#undef MyAlloc
#undef MyFree


#define MyRead(file, buf, size)      \
    AROS_CALL3                       \
    (                                \
        LONG, funcarray[0],          \
        AROS_LCA(BPTR,   file, D1),  \
        AROS_LCA(void *, buf,  D2),  \
        AROS_LCA(LONG,   size, D3),  \
        struct DosLibrary *, DOSBase \
    )


#define MyAlloc(size, flags)        \
    AROS_CALL2                      \
    (                               \
        void *, funcarray[1],       \
        AROS_LCA(ULONG, size,  D0), \
        AROS_LCA(ULONG, flags, D1), \
        struct ExecBase *, SysBase  \
    )				    


#define MyFree(addr, size)          \
    AROS_CALL2NR                    \
    (                               \
        void, funcarray[2],         \
        AROS_LCA(void *, addr, A1), \
        AROS_LCA(ULONG,  size, D0), \
        struct ExecBase *, SysBase  \
    )

static int read_block
(
    BPTR               file,
    ULONG              offset,
    APTR               buffer,
    ULONG              size,
    SIPTR              *funcarray,
    struct DosLibrary *DOSBase
)
{
    UBYTE *buf = (UBYTE *)buffer;
    LONG   subsize;

    if (Seek(file, offset, OFFSET_BEGINNING) < 0)
        return 0;

    while (size)
    {
        subsize = MyRead(file, buf, size);

        if (subsize <= 0)
        {
            if (subsize == 0)
                SetIoErr(ERROR_BAD_HUNK);

            return 0;
        }

        buf  += subsize;
        size -= subsize;
    }

    return 1;
}

static void * load_block
(
    BPTR               file,
    ULONG              offset,
    ULONG              size,
    SIPTR             *funcarray,
    struct DosLibrary *DOSBase
)
{
    D(bug("[ELF Loader] Load Block\n"));
    D(bug("[ELF Loader] (size=%d)\n",size));
    D(bug("[ELF Loader] (funcarray=0x%x)\n",funcarray));
    D(bug("[ELF Loader] (funcarray[1]=0x%x)\n",funcarray[1]));
    void *block = MyAlloc(size, MEMF_ANY);
    if (block)
    {
        if (read_block(file, offset, block, size, funcarray, DOSBase))
            return block;

        MyFree(block, size);
    }
    else
        SetIoErr(ERROR_NO_FREE_STORE);

    return NULL;
}

static int load_header(BPTR file, struct elfheader *eh, SIPTR *funcarray, struct DosLibrary *DOSBase) {
    if (!read_block(file, 0, eh, offsetof(struct elfheader, int_shnum), funcarray, DOSBase))
        return 0;

    if (eh->ident[0] != 0x7f || eh->ident[1] != 'E'  ||
        eh->ident[2] != 'L'  || eh->ident[3] != 'F') {
	D(bug("[ELF Loader] Not an ELF object\n"));
        SetIoErr(ERROR_NOT_EXECUTABLE);
        return 0;
    }
    D(bug("[ELF Loader] ELF object\n"));

    eh->int_shnum = eh->shnum;
    eh->int_shstrndx = eh->shstrndx;

    /* the ELF header only uses 16 bits to store the count of section headers,
     * so it can't handle more than 65535 headers. if the count is 0, and an
     * offset is defined, then the real count can be found in the first
     * section header (which always exists).
     *
     * similarly, if the string table index is SHN_XINDEX, then the actual
     * index is found in the first section header also.
     *
     * see the System V ABI 2001-04-24 draft for more details.
     */
    if (eh->int_shnum == 0 || eh->int_shstrndx == SHN_XINDEX) {
        if (eh->shoff == 0) {
            SetIoErr(ERROR_NOT_EXECUTABLE);
            return 0;
        }

        struct sheader sh;
        if (!read_block(file, eh->shoff, &sh, sizeof(sh), funcarray, DOSBase))
            return 0;

        /* wider section header count is in the size field */
        if (eh->int_shnum == 0)
            eh->int_shnum = sh.size;

        /* wider string table index is in the link field */
        if (eh->int_shstrndx == SHN_XINDEX)
            eh->int_shstrndx = sh.link;

        /* sanity, if they're still invalid then this isn't elf */
        if (eh->int_shnum == 0 || eh->int_shstrndx == SHN_XINDEX) {
            SetIoErr(ERROR_NOT_EXECUTABLE);
            return 0;
        }
    }

    /* WANT_CLASS should be defined for your target */
    if (eh->ident[EI_CLASS]   != WANT_CLASS      ||
        eh->ident[EI_VERSION] != EV_CURRENT      ||
        eh->type              != ET_REL          ||
        eh->ident[EI_DATA]    != WANT_BYTE_ORDER ||
        eh->machine           != AROS_ELF_MACHINE)
    {
        D(bug("[ELF Loader] Object is of wrong type\n"));
        D(bug("[ELF Loader] EI_CLASS   is %d - should be %d\n", eh->ident[EI_CLASS]  , WANT_CLASS     ));
        D(bug("[ELF Loader] EI_VERSION is %d - should be %d\n", eh->ident[EI_VERSION], EV_CURRENT     ));
        D(bug("[ELF Loader] type       is %d - should be %d\n", eh->type             , ET_REL         ));
        D(bug("[ELF Loader] EI_DATA    is %d - should be %d\n", eh->ident[EI_DATA]   , WANT_BYTE_ORDER));
        D(bug("[ELF Loader] machine    is %d - should be %d\n", eh->machine          , AROS_ELF_MACHINE));

        SetIoErr(ERROR_NOT_EXECUTABLE);
        return 0;
    }

    return 1;
}

static int load_hunk
(
    BPTR                 file,
    BPTR               **next_hunk_ptr,
    struct sheader      *sh,
    SIPTR               *funcarray,
    BOOL                 do_align,
    struct DosLibrary   *DOSBase
)
{
    struct hunk *hunk;
    ULONG   hunk_size;

    if (!sh->size)
        return 1;

    /* The size of the hunk is the size of the section, plus
       the size of the hunk structure, plus the size of the alignment (if necessary)*/
    hunk_size = sh->size + sizeof(struct hunk);

    if (do_align)
    {
         hunk_size += sh->addralign;

         /* Also create space for a trampoline, if necessary */
         if (sh->flags & SHF_EXECINSTR)
             hunk_size += sizeof(struct FullJumpVec);
    }

    hunk = MyAlloc(hunk_size, MEMF_ANY | (sh->type == SHT_NOBITS) ? MEMF_CLEAR : 0);
    if (hunk)
    {
        hunk->next = 0;
	hunk->size = hunk_size;

        /* In case we are required to honour alignment, and If this section contains
	   executable code, create a trampoline to its beginning, so that even if the
	   alignment requirements make the actual code go much after the end of the
	   hunk structure, the code can still be reached in the usual way.  */
        if (do_align)
        {
	    if (sh->flags & SHF_EXECINSTR)
            {
	        sh->addr = (char *)AROS_ROUNDUP2
                (
                    (IPTR)hunk->data + sizeof(struct FullJumpVec), sh->addralign
                );
                __AROS_SET_FULLJMP((struct FullJumpVec *)hunk->data, sh->addr);
            }
            else
                sh->addr = (char *)AROS_ROUNDUP2((IPTR)hunk->data, sh->addralign);
	}
	else
	    sh->addr = hunk->data;

        /* Link the previous one with the new one */
        BPTR2HUNK(*next_hunk_ptr)->next = HUNK2BPTR(hunk);

        D(bug("[dos] hunk @ %p\n", sh->addr));
        
        /* Update the pointer to the previous one, which is now the current one */
        *next_hunk_ptr = &hunk->next;

        if (sh->type != SHT_NOBITS)
            return read_block(file, sh->offset, sh->addr, sh->size, funcarray, DOSBase);

        return 1;

    }

    SetIoErr(ERROR_NO_FREE_STORE);

    return 0;
}

static int relocate
(
    struct elfheader  *eh,
    struct sheader    *sh,
    ULONG              shrel_idx,
    struct sheader    *symtab_shndx,
    struct DosLibrary *DOSBase
)
{
    struct sheader *shrel    = &sh[shrel_idx];
    struct sheader *shsymtab = &sh[SHINDEX(shrel->link)];
    struct sheader *toreloc  = &sh[SHINDEX(shrel->info)];

    struct symbol *symtab   = (struct symbol *)shsymtab->addr;
    struct relo   *rel      = (struct relo *)shrel->addr;
    char          *section  = toreloc->addr;

    /* this happens if the target section has no allocation. that can happen
     * eg. with a .debug PROGBITS and a .rel.debug section */
    if (section == NULL)
        return 1;

    ULONG numrel = shrel->size / shrel->entsize;
    ULONG i;

    struct symbol *SysBase_sym = NULL;
    
    for (i=0; i<numrel; i++, rel++)
    {
        struct symbol *sym = &symtab[ELF_R_SYM(rel->info)];
        ULONG *p = (ULONG *)&section[rel->offset];
	IPTR s;
        ULONG shindex;

        if (sym->shindex != SHN_XINDEX)
            shindex = sym->shindex;

        else {
            if (symtab_shndx == NULL) {
                D(bug("[ELF Loader] got symbol with shndx 0xfff, but there's no symtab shndx table\n"));
                SetIoErr(ERROR_BAD_HUNK);
                return 0;
            }
            shindex = ((ULONG *)symtab_shndx->addr)[ELF_R_SYM(rel->info)];
        }

        switch (shindex)
        {

            case SHN_UNDEF:
                D(bug("[ELF Loader] Undefined symbol '%s' while relocating the section '%s'\n",
		      (STRPTR)sh[SHINDEX(shsymtab->link)].addr + sym->name,
		      (STRPTR)sh[SHINDEX(eh->int_shstrndx)].addr + toreloc->name));
                      SetIoErr(ERROR_BAD_HUNK);
                return 0;

            case SHN_COMMON:
                D(bug("[ELF Loader] COMMON symbol '%s' while relocating the section '%s'\n",
		      (STRPTR)sh[SHINDEX(shsymtab->link)].addr + sym->name,
		      (STRPTR)sh[SHINDEX(eh->int_shstrndx)].addr + toreloc->name));
                      SetIoErr(ERROR_BAD_HUNK);
		      
                return 0;

            case SHN_ABS:
	        if (SysBase_sym == NULL)
		{
		    if (strncmp((STRPTR)sh[SHINDEX(shsymtab->link)].addr + sym->name, "SysBase", 8) == 0)
		    {
		        SysBase_sym = sym;
			goto SysBase_yes;
	            }
		    else
		        goto SysBase_no;
	        }
		else
		if (SysBase_sym == sym)
		{
		    SysBase_yes: s = (IPTR)&SysBase;
		}
		else
		    SysBase_no:  s = sym->value;
                break;

  	    default:
		s = (IPTR)sh[SHINDEX(shindex)].addr + sym->value;
 	}

        switch (ELF_R_TYPE(rel->info))
        {
            #if defined(__i386__)

            case R_386_32: /* 32bit absolute */
                *p += s;
                break;

            case R_386_PC32: /* 32bit PC relative */
                *p += s - (ULONG)p;
                break;

            case R_386_NONE:
                break;

            #elif defined(__x86_64__)
            case R_X86_64_64: /* 64bit direct/absolute */
                *(UQUAD *)p = s + rel->addend;
                break;

            case R_X86_64_PC32: /* PC relative 32 bit signed */
                *(ULONG *)p = s + rel->addend - (IPTR) p;
                break;

            case R_X86_64_32:
                *(ULONG *)p = (UQUAD)s + (UQUAD)rel->addend;
                break;
                
            case R_X86_64_32S:
                *(LONG *)p = (QUAD)s + (QUAD)rel->addend;
                break;

            case R_X86_64_NONE: /* No reloc */
                break;
		
            #elif defined(__mc68000__)

            case R_68K_32:
                *p = s + rel->addend;
                break;

            case R_68K_PC32:
                *p = s + rel->addend - (ULONG)p;
                break;

            case R_68k_NONE:
                break;

            #elif defined(__ppc__) || defined(__powerpc__)

            case R_PPC_ADDR32:
                *p = s + rel->addend;
                break;
	
	    case R_PPC_ADDR16_LO:
		{
		    unsigned short *c = (unsigned short *) p;
		    *c = (s + rel->addend) & 0xffff;
		}
		break;
	    
	    case R_PPC_ADDR16_HA:
		{
		    unsigned short *c = (unsigned short *) p;
		    ULONG temp = s + rel->addend;
		    *c = temp >> 16;
		    if ((temp & 0x8000) != 0)
			(*c)++;
		}
		break;
	    
            case R_PPC_REL16_LO:
		{
		    unsigned short *c = (unsigned short *) p;
		    *c = (s + rel->addend - (ULONG) p) & 0xffff;
		}
		break;

            case R_PPC_REL16_HA:
		{
		    unsigned short *c = (unsigned short *) p;
		    ULONG temp = s + rel->addend - (ULONG) p;
		    *c = temp >> 16;
		    if ((temp & 0x8000) != 0)
			(*c)++;
		}
		break;

            case R_PPC_REL24:
		*p &= ~0x3fffffc;
                *p |= (s + rel->addend - (ULONG) p) & 0x3fffffc;
                break;

	    case R_PPC_REL32:
		*p = s + rel->addend - (ULONG) p;
		break;
	    
            case R_PPC_NONE:
                break;
            
            #elif defined(__arm__)
            case R_ARM_CALL:
	    case R_ARM_JUMP24:
    	    case R_ARM_PC24:
            {
                /* On ARM the 24 bit offset is shifted by 2 to the right */
                signed long offset = (*p & 0x00ffffff) << 2;
                /* If highest bit set, make offset negative */
                if (offset & 0x02000000)
                    offset -= 0x04000000;

                offset += s - (uint32_t)p;

                offset >>= 2;
                *p &= 0xff000000;
                *p |= offset & 0x00ffffff;
            }
            break;

    	    case R_ARM_MOVW_ABS_NC:
    	    case R_ARM_MOVT_ABS:
            {
                signed long offset = *p;
                offset = ((offset & 0xf0000) >> 4) | (offset & 0xfff);
                offset = (offset ^ 0x8000) - 0x8000;

                offset += s;

                if (ELF_R_TYPE(rel->info) == R_ARM_MOVT_ABS)
                    offset >>= 16;

                *p &= 0xfff0f000;
                *p |= ((offset & 0xf000) << 4) | (offset & 0x0fff);
            }
            break;

    	    case R_ARM_ABS32:
        	*p += s;
        	break;

    	    case R_ARM_NONE:
        	break;

            #else
            #    error Your architecture is not supported
            #endif

            default:
                bug("[ELF Loader] Unrecognized relocation type %d %d\n", i, ELF_R_TYPE(rel->info));
                SetIoErr(ERROR_BAD_HUNK);
		return 0;
        }
    }

    return 1;
}

BPTR InternalLoadSeg_ELF
(
    BPTR               file,
    BPTR               table __unused,
    SIPTR             *funcarray,
    SIPTR             *stack __unused,
    struct DosLibrary *DOSBase
)
{
    struct elfheader  eh;
    struct sheader   *sh;
    struct sheader   *symtab_shndx = NULL;
    BPTR   hunks         = 0;
    BPTR  *next_hunk_ptr = &hunks;
    ULONG  i;
    BOOL   exec_hunk_seen = FALSE;

    /* load and validate ELF header */
    if (!load_header(file, &eh, funcarray, DOSBase))
        return 0;

    /* load section headers */
    if (!(sh = load_block(file, eh.shoff, eh.int_shnum * eh.shentsize, funcarray, DOSBase)))
        return 0;

    /* load the string table */
    STRPTR st = NULL;
    struct sheader *shstr = sh + SHINDEX(eh.int_shstrndx);
    if (shstr->size != 0)
    {
	st = MyAlloc(shstr->size, MEMF_ANY | MEMF_CLEAR);
        read_block(file, shstr->offset, st, shstr->size, funcarray, DOSBase);
    }

    /* Iterate over the section headers in order to do some stuff... */
    for (i = 0; i < eh.int_shnum; i++)
    {
        /*
           Load the symbol and string table(s).

           NOTICE: the ELF standard, at the moment (Nov 2002) explicitely states
                   that only one symbol table per file is allowed. However, it
                   also states that this may change in future... we already handle it.
        */
        if (sh[i].type == SHT_SYMTAB || sh[i].type == SHT_STRTAB || sh[i].type == SHT_SYMTAB_SHNDX)
        {
            sh[i].addr = load_block(file, sh[i].offset, sh[i].size, funcarray, DOSBase);
            if (!sh[i].addr)
                goto error;

            if (sh[i].type == SHT_SYMTAB_SHNDX) {
                if (symtab_shndx == NULL)
                    symtab_shndx = &sh[i];
                else
                    D(bug("[ELF Loader] file contains multiple symtab shndx tables. only using the first one\n"));
            }
        }
        else
        /* Load the section in memory if needed, and make an hunk out of it */
        if (sh[i].flags & SHF_ALLOC)
        {
	    if (sh[i].size)
	    {
	        /* Only allow alignment if this is an executable hunk
		   or if an executable hunk has been loaded already,
		   so to avoid the situation in which a data hunk has its
		   content displaced from the hunk's header in case it's the
		   first hunk (this happens with Keymaps, for instance).  */
	        if (sh[i].flags & SHF_EXECINSTR)
		    exec_hunk_seen = TRUE;

                if (!load_hunk(file, &next_hunk_ptr, &sh[i], funcarray, exec_hunk_seen, DOSBase))
                    goto error;
	    }
        }

    }

    /* Relocate the sections */
    for (i = 0; i < eh.int_shnum; i++)
    {
        /* Does this relocation section refer to a hunk? If so, addr must be != 0 */
        if ((sh[i].type == AROS_ELF_REL) && sh[SHINDEX(sh[i].info)].addr)
        {
	    sh[i].addr = load_block(file, sh[i].offset, sh[i].size, funcarray, DOSBase);
            if (!sh[i].addr || !relocate(&eh, sh, i, symtab_shndx, DOSBase))
                goto error;

            MyFree(sh[i].addr, sh[i].size);
            sh[i].addr = NULL;
        }
    }

#ifdef KrnRegisterModule
    /* Everything is loaded now. Register the module at kernel.resource */
    if (KernelBase)
    {
	char buffer[512];

	if (NameFromFH(file, buffer, sizeof(buffer))) {
	    char *nameptr = buffer;
	    struct ELF_DebugInfo dbg = {&eh, sh};

/* gdb support needs full paths */
#if !AROS_MODULES_DEBUG
	    /* First, go through the name, till end of the string */
	    while(*nameptr++);
	    /* Now, go back until either ":" or "/" is found */
	    while(nameptr > buffer && nameptr[-1] != ':' && nameptr[-1] != '/')
    		nameptr--;
#endif
    	   KrnRegisterModule(nameptr, hunks, DEBUG_ELF, &dbg);
	}
    }
#endif
    goto end;

error:

    /* There were some errors, deallocate The hunks */

    InternalUnLoadSeg(hunks, (VOID_FUNC)funcarray[2]);
    hunks = 0;

end:
    
    /* Clear the caches to let the CPU see the new data and instructions */
    {
        BPTR *curr = &hunks;
        while (*curr)
        {
             struct hunk *hunk = BPTR2HUNK(curr);
             
	     CacheClearE(hunk->data, hunk->size, CACRF_ClearD | CACRF_ClearI);
             
             curr = hunk->next;
        }
    }
    
    /* deallocate the symbol tables */
    for (i = 0; i < eh.int_shnum; i++)
    {
        if (((sh[i].type == SHT_SYMTAB) || (sh[i].type == SHT_STRTAB)) && (sh[i].addr != NULL))
            MyFree(sh[i].addr, sh[i].size);
    }

    /* Free the string table */
    MyFree(st, shstr->size);

    /* Free the section headers */
    MyFree(sh, eh.int_shnum * eh.shentsize);

    return hunks;
}

#undef MyRead1
#undef MyAlloc
#undef MyFree
