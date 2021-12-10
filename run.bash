set -e
if ! [[ -x $1 ]]
then
    >&2 echo "Usage: bash run.bash <executable> <N>"
    exit 1
fi

if [[ $2 =~ [[:digit:]]+ ]]
then
    N="$2"
else
    N=1
fi

rsync pg.txt /tmp/
rsync ref.txt /tmp/out.txt
md5sum /tmp/out.txt >/tmp/out.txt.md5

sudo find /sys/devices/system/cpu -name scaling_governor -exec sh -c 'echo performance >{}' ';'
sudo sh -c 'echo off >/sys/devices/system/cpu/smt/control ; tuna --cpus=1-7 --isolate'

function on_exit {
    sudo sh -c 'echo on >/sys/devices/system/cpu/smt/control ; tuna --cpus=0-15 --include'
    sudo find /sys/devices/system/cpu -name scaling_governor -exec sh -c 'echo powersave >{}' ';'
}
trap on_exit EXIT

for (( i = 0 ; i < N; ++i ))
do
    rm /tmp/out.txt
    time LC_ALL="C" taskset --cpu-list 1-7 "$1" /tmp/pg.txt /tmp/out.txt
    md5sum -c /tmp/out.txt.md5
done

