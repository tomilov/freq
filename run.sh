[[ -x $1 ]] || exit 1
rsync pg.txt /tmp/pg.txt
export LC_ALL="C"
sudo find /sys/devices/system/cpu -name scaling_governor -exec sh -c 'echo performance >{}' ';'
sudo sh -c 'echo off >/sys/devices/system/cpu/smt/control ; tuna --cpus=1-7 --isolate'
for i in {1..5}
do
    time taskset --cpu-list 1-7 "$1" /tmp/pg.txt /tmp/out.txt && md5sum reference.txt /tmp/out.txt
done
sudo sh -c 'echo on >/sys/devices/system/cpu/smt/control ; tuna --cpus=0-15 --include'
sudo find /sys/devices/system/cpu -name scaling_governor -exec sh -c 'echo powersave >{}' ';'
