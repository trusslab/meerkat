#!/bin/bash

count() {
    echo "$(git log --numstat --pretty="%H" $1..$2 $3 | awk 'NF==3 {plus+=$1; minus+=$2} END {printf("+%d, -%d\n", plus, minus)}')"
}

y17="9ae81923f5fc397e5f0cf2836926a7d1cab26aa5"
y18="dabc25ee28d36fe572d2093224db0d02e5dc8da4"
y19="0dd18ffb54df420d2c7d25bc5259942dbbc2a610"
y20="31724e5503a9ffd71c440c31a1e3d6a50bd50186"
y21="8abc7aa5d31067372903fb523ed96e8470b92d50"
y22="da629478930871e4c9a51c56ad86078f10b83f9f"
ynw="ca4582c286aa4465f9d1a72bef34b04ee907d42e"

dir=$1

echo "2017: $(count $y17 $y18 $dir)"
echo "2018: $(count $y18 $y19 $dir)"
echo "2019: $(count $y19 $y20 $dir)"
echo "2020: $(count $y20 $y21 $dir)"
echo "2021: $(count $y21 $y22 $dir)"
echo "2022: $(count $y22 $ynw $dir)"

