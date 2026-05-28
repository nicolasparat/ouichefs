# Workflow (from the ouichefs directory)

```
make clean
make KERNELDIR=../linux-6.5.7/
cd mkfs
make clean
make
rm -f test.img
dd if=/dev/zero of=test.img bs=1M count=50
./mkfs.ouichefs test.img
cp ../ouichefs.ko ../../TME2/share/
cd ../../TME2
./qemu-run-asus.sh
insmod /share/ouichefs.ko
mount -t ouichefs /dev/sdc /mnt
```

# Tests

```
touch test1.txt
echo "AAA" > test1.txt
cat test1.txt
echo "BBB" >> test1.txt
cat test1.txt
echo "CCC" > test1.txt
cat test1.txt
rm test1.txt
```

```
<!-- Partition fraîche, écris un fichier de 6 MB (depuis /, pas /mnt) -->
dd if=/dev/urandom of=/mnt/big.bin bs=1M count=6

<!-- Vérifie les extents : avec 1.6, on devrait avoir 1 ou très peu d'extents -->
./test_extents /mnt/big.bin
dmesg | tail -5
<!-- Attendu : 1 extent(s) avec count=1536 (6MB / 4KB) -->

<!-- Vérifie l'intégrité -->
md5sum /mnt/big.bin
cp /mnt/big.bin /tmp/big_copy.bin
md5sum /tmp/big_copy.bin  <!-- doit matcher -->
```

NB: ./test_extents est le programme utilisant l'ioctl pour display les extents.

```
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
```