#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_EXTENTS _IO(OUICHEFS_IOC_MAGIC, 1)

int main(int argc, char *argv[])
{
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fichier>\n", argv[0]);
        return 1;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    if (ioctl(fd, OUICHEFS_IOC_GET_EXTENTS) < 0)
        perror("ioctl");
    close(fd);
    printf("Resultat dans dmesg\n");
    return 0;
}