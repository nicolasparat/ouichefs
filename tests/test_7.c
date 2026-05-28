#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_EXTENTS _IO(OUICHEFS_IOC_MAGIC, 1)

#define BLOCK 1024

static void fill_random(char *buf, size_t len)
{
    int r = open("/dev/urandom", O_RDONLY);
    if (r < 0) {
        perror("open /dev/urandom");
        exit(1);
    }

    if (read(r, buf, len) != (ssize_t)len) {
        perror("read urandom");
        close(r);
        exit(1);
    }

    close(r);
}

static void write_blocks(int fd, size_t blocks)
{
    char buf[BLOCK];

    for (size_t i = 0; i < blocks; i++) {
        fill_random(buf, BLOCK);

        ssize_t w = write(fd, buf, BLOCK);
        if (w != BLOCK) {
            perror("write");
            exit(1);
        }
    }
}

static void test_extents(int fd)
{
    if (ioctl(fd, OUICHEFS_IOC_GET_EXTENTS) < 0)
        perror("ioctl");
}

int main(void)
{
    const char *a = "/mnt/a.bin";
    const char *b = "/mnt/b.bin";

    /*
     * ==================================================
     * OPEN ONCE → KEEP FD ALIVE FOR ENTIRE TEST
     * ==================================================
     */
    int fd_a = open(a, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_a < 0) {
        perror("open a");
        return 1;
    }

    int fd_b = open(b, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_b < 0) {
        perror("open b");
        return 1;
    }

    /*
     * =========================
     * Test du 1.7
     * =========================
     */

    /* Step 1 */
    write_blocks(fd_a, 4);
    test_extents(fd_a);

    /* Step 2 */
    write_blocks(fd_b, 4);

    /* Step 3 */
    write_blocks(fd_a, 4);
    test_extents(fd_a);
    printf("<!-- Pas d'augmentation d'extent (toujours 1) -->\n");

    /* Step 4 */
    write_blocks(fd_b, 400);
    write_blocks(fd_a, 400);

    test_extents(fd_a);
    printf("<!-- Augmentation d'extent (maintenant 2) -->\n");

    /*
     * FINAL CLOSE (only here release() is called once)
     */
    close(fd_a);
    close(fd_b);

    printf("\nResultat dans dmesg\n");
    return 0;
}