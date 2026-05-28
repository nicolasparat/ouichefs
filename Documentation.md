# Workflow (from the ouichefs directory)

```
git pull
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

Test du 1.2, 1.3, 1.4 et 1.5

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

Test du 1.6

```
<!-- Partition fraîche, écrit un fichier de 6 MB (depuis /, pas /mnt) -->
dd if=/dev/urandom of=/mnt/big.bin bs=1M count=6

./test_extents /mnt/big.bin

<!-- Attendu : 1 extent(s) avec count=1536 (6MB / 4KB) -->
dmesg | tail -5

<!-- Vérifie l'intégrité -->
md5sum /mnt/big.bin
cp /mnt/big.bin /tmp/big_copy.bin
md5sum /tmp/big_copy.bin  <!-- doit matcher -->
```

```
dd if=/dev/urandom of=/mnt/a.bin bs=1M count=6
dd if=/dev/urandom of=/mnt/b.bin bs=1M count=6
dd if=/dev/urandom of=/mnt/a.bin bs=1M count=6 oflag=append conv=notrunc
dd if=/dev/urandom of=/mnt/b.bin bs=1M count=6 oflag=append conv=notrunc
./test_extents /mnt/a.bin
./test_extents /mnt/b.bin

<!-- Les extends devraient être interleaved. -->
```

Test du 1.7

```
./test_7
```

NB : Les programmes exécutés pour les tests (comme ./test_extents et ./test_7) sont dans le répertoire /tests et doivent être compilés et exécutés dans le root de la VM pour faire le test.