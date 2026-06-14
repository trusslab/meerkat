#!/bin/bash

self=${0}

log() {
    echo "${self}: $@"
}

createFile() {
    echo "#include <stdio.h>" > example.c
    echo "int main(void) {" >> example.c
    echo "    printf(\"Hello World\");" >> example.c
    echo "    return 0;" >> example.c
    echo "}" >> example.c 
}

versions=("gcc-10.1.0" "gcc-5.5.0" "gcc-7.3.0" "gcc-8.1.0")

log "Verifying ${#versions[@]} gcc versions"
for v in ${versions[@]}; do
    createFile
    compilers/${v}/bin/gcc example.c -o example
    if (( $? != 0)) || [ ! -f example ]; then
        log "${v} failed to compile an example program!"
        exit
    fi
    rm -f example example.c
done

log "All versions verified."

