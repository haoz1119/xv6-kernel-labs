#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmap.h"
#include "fcntl.h"

#define TEST_CALL(call, return_type, fail_return) { \
    return_type ret; \
    if ((ret = call) == fail_return) { \
        printf(1, "Error at line %d: function returned %d but should not fail\n", __LINE__, (int)ret); \
        printf(1, "TEST FAILED\n"); \
        exit(); \
    } \
}

#define TEST_FAIL(call, return_type, fail_return) { \
    return_type ret; \
    if ((ret = call) != fail_return) { \
        printf(1, "Error at line %d: function returned %d but should fail\n", __LINE__, (int)ret); \
        printf(1, "TEST FAILED\n"); \
        exit(); \
    } \
}

#define MMAP_CALL(call) TEST_CALL(call, char *, (void *)-1)
#define MMAP_FAIL(call) TEST_FAIL(call, char *, (void *)-1)
#define MUNMAP_CALL(call) TEST_CALL(call, int, -1)
#define MUNMAP_FAIL(call) TEST_FAIL(call, int, -1)

// From test_7
int my_strcmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}

int test_7() {
    char *filename = "test_file.txt";
    int len = 100;
    char buff[len];
    char new_buff[len];
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;

    /* Open a file */
    int fd = open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(1, "Error opening file\n");
    goto failed;
    }

    /* Write some data to the file */
    for (int i = 0; i < len; i++) {
        buff[i] = 'x';
    }
    if (write(fd, buff, len) != len) {
        printf(1, "Error: Write to file FAILED\n");
    goto failed;
    }

    /* mmap the file */
    void *mem = mmap(0, len, prot, flags, fd, 0);
    if (mem == (void *)-1) {
        printf(1, "mmap FAILED\n");
    goto failed;
    }

    /* modify in-memory contents of the mmapped region */
    char *mem_buff = (char *)mem;
    for (int i = 0; i < len; i++) {
        mem_buff[i] = 'a';
        buff[i] = mem_buff[i]; // Later used for validating the data returned by read()
    }

    int ret = munmap(mem, len);
    if (ret < 0) {
        printf(1, "munmap FAILED\n");
    goto failed;
    }

    close(fd);

    /* Reopen the file */
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        printf(1, "Error reopening file\n");
    goto failed;
    }

    /* Verify that modifications made to mmapped memory have been reflected in the file */
    if (read(fd, new_buff, len) != len) {
        printf(1, "Read from file FAILED\n");
    goto failed;
    }
    if (my_strcmp(new_buff, buff, len) != 0) {
        printf(1, "Writes to mmaped memory not reflected in file\n");
        printf(1, "\tExpected: %s\n", buff);
        printf(1, "\tGot: %s\n", new_buff);
    goto failed;
    }

    /* Clean and return */
    close(fd);

// success:
    printf(1, "MMAP\t SUCCESS\n");
    return 0;

failed:
    printf(1, "MMAP\t FAILED\n");
    return -1;

}

int main(int argc, char *argv[]) {
    // ====================== MAP_FIXED tests ======================
    // Address less than MMAPBASE
    MMAP_FAIL(mmap((void *)0x50001000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Address greater than MMAPBASE
    MMAP_FAIL(mmap((void *)0x80000000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Address not page aligned
    MMAP_FAIL(mmap((void *)0x70000100, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Normal mapping
    MMAP_CALL(mmap((void *)0x70000000, 100, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Mapping already exists
    MMAP_FAIL(mmap((void *)0x70000000, 100, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Normal mapping: less than first
    MMAP_CALL(mmap((void *)0x6f000000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Normal mapping: between 0 and 1
    MMAP_CALL(mmap((void *)0x6ff00000, 0x2000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Covered by existing mapping
    MMAP_FAIL(mmap((void *)0x6ff01000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // At the boundary of existing mapping
    MMAP_CALL(mmap((void *)0x6ff02000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Normal mapping: greater than last (and touching boundary)
    MMAP_CALL(mmap((void *)0x7ffff000, 0x1000, PROT_READ, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    
    // ====================== munmap tests ======================
    // Address not page aligned
    MUNMAP_FAIL(munmap((void *)0x6f000100, 0x1000));
    // Delete part of a mapping
    MUNMAP_CALL(munmap((void *)0x6ff00000, 1));
    // Delete the rest of previous mapping
    MUNMAP_CALL(munmap((void *)0x6ff01000, 1));
    // Delete non-exist mapping
    MUNMAP_CALL(munmap((void *)0x6ff00000, 0x2000));

    // ====================== test 3: lazy allocation ======================
    uint addr = 0x60020000;
    int len = 4000;
    MMAP_CALL(mmap((void *)addr, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    char *memchar = (char *)addr;
    printf(1, "before accessing memchar[0]\n");
    memchar[0] = 'a';
    printf(1, "before accessing memchar[1]\n");
    memchar[1] = '\0';
    printf(1, "write done: %s\n", memchar);
    TEST_CALL(strcmp(memchar, "a") == 0, int, 0);
    MUNMAP_CALL(munmap((void *)addr, len));

    // ====================== !MAP_FIXED tests ======================
    // TODO: may differ between implementations
    // Length too large
    MMAP_FAIL(mmap((void *)0, 0x30000000, PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    // Map at start
    MMAP_CALL(mmap((void *)0, 1, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    // Map after it for 4 pages
    MMAP_CALL(mmap((void *)0, 0x4000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    memchar = (char *)0x60004ffc;
    memchar[0] = '5';
    memchar[1] = '3';
    memchar[2] = '7';
    memchar[3] = '\0';
    TEST_CALL(strcmp(memchar, "537") == 0, int, 0);
    MUNMAP_CALL(munmap((void *)0x60000000, 0x2000));
    // Map at start again
    MMAP_CALL(mmap((void *)0, 0x2000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    memchar = (char *)0x60001ffe;
    memchar[0] = '$';
    memchar[1] = '\0';
    TEST_CALL(strcmp(memchar, "$") == 0, int, 0);
    // Map with a gap
    MMAP_CALL(mmap((void *)0x60008000, 0x2000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Should just fit in the gap
    TEST_FAIL(mmap((void *)0, 0x3000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0), void *, (void *)0x60005000);

    // ====================== test 7: write back when unmapping ======================
    // Init: unmap everything first
    MUNMAP_CALL(munmap((void *)0x60000000, 0x20000000));

    TEST_CALL(test_7(), int, -1);

    // ====================== MAP_GROWSUP tests ======================
    // Init: unmap everything first
    MUNMAP_CALL(munmap((void *)0x60000000, 0x20000000));

    // 8 + 1 == 9 pages should be occupied, 0x60000000 to 0x60008fff (inclusive)
    MMAP_CALL(mmap((void *)0x60000000, 0x8000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED | MAP_GROWSUP, -1, 0));
    // So this should fail
    MMAP_FAIL(mmap((void *)0x60008000, 0x8000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED | MAP_SHARED, -1, 0));
    // Touch the guard page
    memchar = (char *)0x60008ffe;
    memchar[0] = 'a';
    memchar[1] = '\0';
    TEST_CALL(strcmp(memchar, "a") == 0, int, 0);
    // Should start at 0x6000a000, end at 0x6000afff (inclusive)
    MMAP_CALL(mmap((void *)0, 1, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    memchar = (char *)0x6000affe;
    memchar[0] = 'b';
    memchar[1] = '\0';
    TEST_CALL(strcmp(memchar, "b") == 0, int, 0);

    // TODO: add more tests

    printf(1, "All tests passed!\n");
    exit();
}
