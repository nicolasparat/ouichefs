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

NB: ./test_extents est le programme utilisant l'ioctl pour display les extents.

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