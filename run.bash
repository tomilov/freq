#! /usr/bin/bash

set -e

if [[ ! -f pg.txt ]]
then
    >&2 echo "File pg.txt is not found in current directory"
    exit 2
fi

if [[ ! -x $1 ]]
then
    >&2 echo "Usage: bash run.bash ABSOLUTE_PATH_TO_EXECUTABLE [N]"
    exit 3
fi

if [[ ! $1 = /* ]]
then
    >&2 echo "Value '$1' of the first parameter is not an absolute path"
    exit 4
fi

if [[ $2 ]]
then
    if [[ ! $2 =~ ^[[:digit:]]+$ ]]
    then
        >&2 echo "Value '$2' of the second parameter is not a number"
        exit 5
    fi
    N=$2
else
    N=1
fi

NPROC="$( nproc )"

if ! WORKSPACE="$( mktemp -d --tmpdir 'freq.XXXXXX' )"
then
    >&2 echo "Unable to create temporary directory"
    exit 6
fi

function on_exit {
    set +e
    set -v
    popd >/dev/null

    if [[ $( cat /sys/devices/system/cpu/smt/control ) == "off" ]]
    then
        echo on | sudo tee /sys/devices/system/cpu/smt/control >/dev/null
    fi
    if command -v tuna >/dev/null
    then
        sudo -A tuna --cpus=0-$NPROC --include
    fi
    echo powersave | sudo -A tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null

    rm -r "$WORKSPACE"
}
trap on_exit EXIT

cp -a pg.txt "$WORKSPACE"
pushd "$WORKSPACE"

echo performance | sudo -A tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null
if [[ NPROC > 1 ]]
then
    if [[ $( cat /sys/devices/system/cpu/smt/control ) == "on" ]]
    then
        echo off | sudo -A tee /sys/devices/system/cpu/smt/control >/dev/null
    fi
    if command -v tuna >/dev/null
    then
        sudo -A tuna --cpus=1-$NPROC --isolate
    fi
fi

for (( i = 0 ; i < N ; ++i ))
do
    time LC_ALL=C taskset --cpu-list 1-$NPROC "$1" pg.txt out.txt
    >&2 echo -n
    md5sum --check <( echo '850944413ba9fd1dbf2b9694abaa930d  -' ) <out.txt
    rm out.txt
done
