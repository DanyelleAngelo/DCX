#!/bin/bash
GREEN='\033[0;32m'
BLUE='\033[34m'
RESET='\033[0m'

PIZZA_DIR="../dataset/pizza_chilli"
COMPRESSED_DIR="../dataset/compressed_files"
FILE_PATHS=$(cat ../file_names.txt)
PIZZA_URL="http://pizzachili.dcc.uchile.cl/repcorpus"
EXECUTABLE="../compressor/./main"

check_and_create_folder() {
    echo -e "\n\n${GREEN}%%% Creating directories for files in case don't exist ${RESET}."
    if [ ! -d "$PIZZA_DIR" ]; then
        mkdir -p "$PIZZA_DIR"
    fi
    if [ ! -d "$COMPRESSED_DIR" ]; then
        mkdir -p "$COMPRESSED_DIR"
    fi
}

download_files() {
    echo -e "\n${GREEN}%%% Download files from a list, then descompress the files and remove the compressed files.${RESET}."

    for file in $FILE_PATHS; do
        IFS="/" read -ra file_name <<< "$file"
        compressed_files="$PIZZA_DIR/${file_name[1]}.gz"
        descompressed_file="$PIZZA_DIR/${file_name[1]}"

        if [ ! -e "$compressed_files" ] && [ ! -e "$descompressed_file" ]; then
            echo -e "\n${BLUE}  % $PIZZA_URL/$file.gz ${RESET}"
            curl --output "$compressed_files" "$PIZZA_URL/$file.gz"
            gzip -d "$compressed_files"
        elif [ ! -e "$descompressed_file" ]; then
            echo -e "\n${BLUE}  % $PIZZA_URL/$file.gz ${RESET}"
            gzip -d "$compressed_files"
        fi
    done
}

compress_and_validate() {
    echo -e "\n${GREEN}%%% Validating DCX: Compile files to generate a grammar with rules stored as integer ${RESET}."

    make clean -C ../compressor/
    make compile -C ../compressor/

    coverageList=(3 4 5 6 7 8 9 15 30 60) 

    for plain_file in $FILE_PATHS; do
        IFS="/" read -ra file_name <<< "$plain_file"
        in_plain="$PIZZA_DIR/${file_name[1]}"

        for coverage in "${coverageList[@]}"; do
            out_compressed="$COMPRESSED_DIR/${file_name[1]}-coverage$coverage"
            out_descompressed=$out_compressed-plain
        
            echo -e "\n${BLUE}####### FILE: ${file_name[1]} RULE SIZE: $coverage ${RESET}"
            #compress
            make run_compressor MODE=c COVERAGE=$coverage FILE_IN=$in_plain FILE_OUT=$out_compressed -C ../compressor/
            #decompress
            make run_compressor MODE=d COVERAGE=$coverage FILE_IN=$out_compressed.dcx FILE_OUT=$out_descompressed  ORIGINAL=$in_plain -C ../compressor/
        done
    done
    make clean -C ../compressor/
}

if [ "$0" = "$BASH_SOURCE" ]; then
    check_and_create_folder
    download_files
    compress_and_validate
fi