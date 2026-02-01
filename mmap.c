#include "types.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "mmap.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

#define RET_FAIL (void *)-1

// Allocate a physical page and map the virtual address to it.
// For file-backed mapping, load the data to virtual address.
int alloc_and_map_page(struct proc *p, struct mmap *map, uint addr) {
    char *phy_page = kalloc();
    if (!phy_page)
      return -1;
    memset(phy_page, 0, PGSIZE);

    // Check file-backed mapping
    if (!(map->flags & MAP_ANON)) {
      map->f->off = 0;
      fileread(map->f, phy_page, PGSIZE);
    }

    // Check if it's a copy-on-write page
    pte_t *pte = walkpgdir(p->pgdir, (void *)addr, 0);
    if (pte) {
        *pte = 0;
    }

    if (mappages(p->pgdir, (void *)addr, PGSIZE, V2P(phy_page), map->prot | PTE_U)) {
      kfree(phy_page);
      return -1;
    }

    return 0;
}

// Insert mapping at given index
void insert_mapping(struct proc *p, uint addr, int length, int index) {
    for (int i = p->cur_mappings - 1; i >= index; --i) {
        memmove(&p->map[i+1], &p->map[i], sizeof(struct mmap));
    }
    p->map[index].addr = addr;
    p->map[index].length = length;
    ++(p->cur_mappings);
}

int check_address(struct proc *p, uint addr, int length) {
    if (p->cur_mappings == 0)
        return 0;
    uint upper = PGROUNDUP(addr + length);
    uint lower = PGROUNDUP(addr);

    if (upper <= PGROUNDUP(p->map[0].addr)) {
        return 0;
    }

    for (int i = 0; i < p->cur_mappings - 1 && PGROUNDUP(p->map[i].addr) < upper; ++i) {
        if (lower >= PGROUNDUP(p->map[i].addr + p->map[i].length) &&
            upper <= PGROUNDUP(p->map[i+1].addr))
            return i + 1;
    }

    if (lower >= PGROUNDUP(p->map[p->cur_mappings - 1].addr + p->map[p->cur_mappings - 1].length))
        return p->cur_mappings;
    return -1;
}

int find_address(struct proc *p, int length) {
    if (p->cur_mappings == 0) {
        if (PGROUNDUP(MMAPBASE + length) > KERNBASE)
            return -1;
        insert_mapping(p, MMAPBASE, length, 0);
        return 0;
    }

    if (PGROUNDUP(MMAPBASE + length) <= PGROUNDUP(p->map[0].addr)) {
        insert_mapping(p, MMAPBASE, length, 0);
        return 0;
    }

    for (int i = 0; i < p->cur_mappings; ++i) {
        uint lower = PGROUNDUP(p->map[i].addr + p->map[i].length);
        uint upper = (i == p->cur_mappings - 1) ? KERNBASE : PGROUNDUP(p->map[i+1].addr);
        if (PGROUNDUP(lower + length) <= upper) {
            insert_mapping(p, lower, length, i + 1);
            return i + 1;
        }
    }
    return -1;
}

void *mmap(void *addr, int length, int prot, int flags, struct file *f, int offset) {
    if (length < 1)
        return RET_FAIL;

    // One of MAP_SHARED or MAP_PRIVATE must be specified
    if (!(!(flags & MAP_SHARED) ^ !(flags & MAP_PRIVATE)))
        return RET_FAIL;

    struct proc *p = myproc();
    if (p->cur_mappings >= MMAP_LIMIT)
        return RET_FAIL;

    uint uaddr = (uint)addr;

    if (!(flags & MAP_ANON)) {
        if (!f->readable || ((flags & MAP_SHARED) && (prot & PROT_WRITE) && !f->writable))
            return RET_FAIL;
    }

    // Add guard page if MAP_GROWSUP is set
    if (flags & MAP_GROWSUP)
        length += PGSIZE;

    void *ret = (void *)0;
    int index = -1;
    if (flags & MAP_FIXED) {
        if (uaddr < MMAPBASE)
            return RET_FAIL;
        if (uaddr % PGSIZE)
            return RET_FAIL;
        if (PGROUNDUP(uaddr + length) > KERNBASE)
            return RET_FAIL;
        index = check_address(p, uaddr, length);
        if (index == -1)
            return RET_FAIL;
        insert_mapping(p, uaddr, length, index);
        ret = addr;
    } else {
        index = find_address(p, length);
        if (index == -1)
            return RET_FAIL;
        ret = (void *)p->map[index].addr;
    }

    p->map[index].f = f;
    p->map[index].prot = prot;
    p->map[index].flags = flags;

    return ret;
}

int munmap(void *addr, int length) {
    struct proc *p = myproc();
    uint uaddr = (uint)addr;

    if (uaddr % PGSIZE)
        return -1;

    uaddr = PGROUNDUP((uint)addr);
    uint lower = uaddr;
    uint upper = PGROUNDUP(uaddr + length);

    int start = -1, end = -1;
    for (int i = 0; i < p->cur_mappings; ++i) {
        uint cur_lower = PGROUNDUP(p->map[i].addr);
        uint cur_upper = PGROUNDUP(p->map[i].addr + p->map[i].length);
        if (cur_upper > lower && cur_lower < upper) {
            if (start == -1)
                start = i;
            end = i;

            // Write back if not anon and shared
            if (!(p->map[i].flags & MAP_ANON) && (p->map[i].flags & MAP_SHARED) && (p->map[i].prot & PROT_WRITE)) {
                p->map[i].f->off = 0;
                if (filewrite(p->map[i].f, (char *)p->map[i].addr, p->map[i].length - (p->map[i].flags & MAP_GROWSUP ? PGSIZE : 0)) < 0) {
                    return -1;
                }
            }

            // Free pages
            for (uint cur = cur_lower; cur < cur_upper; cur += PGSIZE) {
                if (!(cur >= lower && cur < upper)) continue;
                pte_t *pte = walkpgdir(p->pgdir, (void *)cur, 0);
                if (!pte) continue;
                if ((p->map[i].flags & MAP_SHARED) && (p->map[i].prot & PROT_CHILD)) {
                    *pte &= ~PTE_P;
                } else if (PTE_ADDR(*pte)) {
                    kfree(P2V(PTE_ADDR(*pte)));
                    *pte = 0;
                }
            }
        }
    }
    if (end == -1) {
        return 0;
    }

    // Handle mapping removal/splitting
    if (start == end) {
        uint cur_lower = PGROUNDUP(p->map[end].addr);
        uint cur_upper = PGROUNDUP(p->map[end].addr + p->map[end].length);
        if (lower > cur_lower && upper < cur_upper) {
            if (p->cur_mappings == MMAP_LIMIT) {
                return -1;
            } else {
                insert_mapping(p, upper, cur_upper - upper, end + 1);
                p->map[end + 1].prot = p->map[end].prot;
                p->map[end + 1].f = p->map[end].f;
                p->map[end + 1].flags = p->map[end].flags;
                p->map[end].length = lower - cur_lower;
            }
        } else if (cur_lower >= lower && cur_upper <= upper) {
            for (int i = end; i < p->cur_mappings - 1; ++i) {
                memmove(&p->map[i], &p->map[i+1], sizeof(struct mmap));
            }
            --(p->cur_mappings);
        } else {
            if (cur_upper > upper) {
                p->map[end].length -= (upper - cur_lower);
                p->map[end].addr = upper;
            } else {
                p->map[end].length = lower - cur_lower;
            }
        }
    } else {
        int mappings_to_remove = end - start - 1;
        int remove_start = 0;
        if (PGROUNDUP(p->map[start].addr) >= lower) {
            ++mappings_to_remove;
            remove_start = 1;
        } else {
            p->map[start].length = lower - PGROUNDUP(p->map[start].addr);
        }
        if (PGROUNDUP(p->map[end].addr + p->map[end].length) <= upper) {
            ++mappings_to_remove;
        } else {
            p->map[end].length -= (upper - p->map[end].addr);
            p->map[end].addr = upper;
        }
        for (int i = (remove_start ? start : start + 1); mappings_to_remove && i < p->cur_mappings - mappings_to_remove; ++i) {
            memmove(&p->map[i], &p->map[i+mappings_to_remove], sizeof(struct mmap));
        }
        p->cur_mappings -= mappings_to_remove;
    }

    return 0;
}
