#!/bin/bash

count() {
    echo "$(git log --numstat --pretty="%H" $1..$2 $3 | awk 'NF==3 {plus+=$1; minus+=$2} END {printf("+%d, -%d\n", plus, minus)}')"
}

# Q1: Jan-Mar 01
# Q2: Apr-Jun 04
# Q3: Jul-Sep 07
# Q4: Oct-Dec 10

# This is just the worst way to do this, but it makes it really clear what's going on
y17q3="9ae81923f5fc397e5f0cf2836926a7d1cab26aa5" # 4.13-rc2 2017-07-23
y17q4="9e66317d3c92ddaab330c125dfe9d06eee268aff" # 4.14-rc3 2017-10-01
y18q1="dabc25ee28d36fe572d2093224db0d02e5dc8da4" # 4.15-rc6 2017-12-31
y18q2="0adb32858b0bddf4ada5f364a84ed60b196dbcda" # 4.16     2018-04-01
y18q3="021c91791a5e7e85c567452f1be3e4c2c6cb6063" # 4.18-rc3 2018-07-01
y18q4="17b57b1883c1285f3d0dc2266e8f79286a7bef38" # 4.19-rc6 2019-09-30
y19q1="0dd18ffb54df420d2c7d25bc5259942dbbc2a610" # 5.0-rc1  2019-01-06
y19q2="79a3aaa7b82e3106be97842dedfd8429248896e6" # 5.1-rc3  2019-03-31
y19q3="6fbc7275c7a9ba97877050335f290341a1fd8dbf" # 5.2-rc7  2019-06-30
y19q4="54ecb8f7028c5eb3d740bb82b0f1d90f2df63c5c" # 5.4-rc1  2019-09-30
y20q1="31724e5503a9ffd71c440c31a1e3d6a50bd50186" # 5.5-rc4  2019-12-29
y20q2="7111951b8d4973bda27ff663f2cf18b663d15b48" # 5.6      2020-03-29
y20q3="9ebcfadb0610322ac537dd7aa5d9cbc2b2894c68" # 5.8-rc3  2020-06-28
y20q4="a1b8638ba1320e6684aa98233c15255eb803fac7" # 5.9-rc7  2020-09-27
y21q1="8abc7aa5d31067372903fb523ed96e8470b92d50" # 5.11-rc2 2021-01-03
y21q2="e49d033bddf5b565044e2abe4241353959bc9120" # 5.12-rc6 2021-04-04
y21q3="62fb9874f5da54fdb243003b386128037319b219" # 5.13     2021-06-27
y21q4="9e1ff307c779ce1f0f810c7ecce3d95bbae40896" # 5.15-rc4 2021-10-03
y22q1="da629478930871e4c9a51c56ad86078f10b83f9f" # 5.16-rc8 2022-01-02
y22q2="3123109284176b1532874591f7c81f3837bbdc17" # 5.18-rc1 2022-04-03
y22q3="88084a3df1672e131ddc1b4e39eeacfd39864acf" # 5.19-rc5 2022-07-03
y22q4="4fe89d07dcc2804c8b562f6c7896a45643d34b2f" # 6.0      2022-10-02

dir=$1

echo "2017 Q3: $(count $y17q3 $y17q4 $dir)"
echo "2017 Q4: $(count $y17q4 $y18q1 $dir)"
echo "2018 Q1: $(count $y18q1 $y18q2 $dir)"
echo "2018 Q2: $(count $y18q2 $y18q3 $dir)"
echo "2018 Q3: $(count $y18q3 $y18q4 $dir)"
echo "2018 Q4: $(count $y18q4 $y19q1 $dir)"
echo "2019 Q1: $(count $y19q1 $y19q2 $dir)"
echo "2019 Q2: $(count $y19q2 $y19q3 $dir)"
echo "2019 Q3: $(count $y19q3 $y19q4 $dir)"
echo "2019 Q4: $(count $y19q4 $y20q1 $dir)"
echo "2020 Q1: $(count $y20q1 $y20q2 $dir)"
echo "2020 Q2: $(count $y20q2 $y20q3 $dir)"
echo "2020 Q3: $(count $y20q3 $y20q4 $dir)"
echo "2020 Q4: $(count $y20q4 $y21q1 $dir)"
echo "2021 Q1: $(count $y21q1 $y21q2 $dir)"
echo "2021 Q2: $(count $y21q2 $y21q3 $dir)"
echo "2021 Q3: $(count $y21q3 $y21q4 $dir)"
echo "2021 Q4: $(count $y21q4 $y22q1 $dir)"
echo "2022 Q1: $(count $y22q1 $y22q2 $dir)"
echo "2022 Q2: $(count $y22q2 $y22q3 $dir)"
echo "2022 Q3: $(count $y22q3 $y22q4 $dir)"
