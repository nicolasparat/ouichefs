#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_EXTENTS _IO(OUICHEFS_IOC_MAGIC, 1)

int main(void)
{
    int fd_a = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_a < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_a, 51380200) != 0) {
        perror("ftruncate");
        close(fd_a);
        return 1;
    }
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_EXTENTS _IO(OUICHEFS_IOC_MAGIC, 1)

int main(void)
{
    int fd_a = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_a < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_a, 51380200) != 0) {
        perror("ftruncate");
        close(fd_a);
        return 1;
    }

    
    close(fd_a);
    printf("Taille de la reservation print dans dmesg - 1");

    int fd_b = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_b < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_b, 51380200) != 0) {
        perror("ftruncate");
        close(fd_b);
        return 1;
    }

    
    close(fd_b);
    printf("Taille de la reservation print dans dmesg - 2");

    int fd_c = open("/mnt/d.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_c < 0) {
        perror("open b");
        return 1;
    }

    if(ftruncate(fd_c, 5) != 0) {
        perror("ftruncate");
        close(fd_c);
        return 1;
    }

    close(fd_c);

    int fd_d = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_d < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_d, 51380200) != 0) {
        perror("ftruncate");
        close(fd_d);
        return 1;
    }

    
    close(fd_d);
    printf("Taille de la reservation print dans dmesg - 3");
    return 0;
}
    
    close(fd_a);
    printf("Taille de la reservation print dans dmesg - 1");

    int fd_b = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_b < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_b, 51380200) != 0) {
        perror("ftruncate");
        close(fd_b);
        return 1;
    }

    
    close(fd_b);
    printf("Taille de la reservation print dans dmesg - 2");

    int fd_c = open("/mnt/d.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_c < 0) {
        perror("open b");
        return 1;
    }

    if(ftruncate(fd_c, 5) != 0) {
        perror("ftruncate");
        close(fd_c);
        return 1;
    }

    close(fd_c);

    int fd_d = open("/mnt/c.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_d < 0) {
        perror("open a");
        return 1;
    }

    if(ftruncate(fd_d, 51380200) != 0) {
        perror("ftruncate");
        close(fd_d);
        return 1;
    }

    
    close(fd_d);
    printf("Taille de la reservation print dans dmesg - 3");
    return 0;
}