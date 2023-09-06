#!/bin/bash
GREEN='\033[0;32m'
BLUE='\033[34m'
RESET='\033[0m'
CURRENT_DATE=$(date +"%Y-%m-%d")

#url to download the files
PIZZA_URL="http://pizzachili.dcc.uchile.cl/repcorpus"
#directories
REPORT_DIR="../report"
PIZZA_DIR="../dataset/pizza_chilli"
COMPRESSED_DIR="../dataset/compressed_files"
COVERAGE_LIST=(3 4 5 6 7 8 9 15 30 60)


GENERAL_REPORT="$REPORT_DIR/$CURRENT_DATE-general-report-errors.csv"
FILE_PATHS=$(cat ../file_names.txt)
HEADER="file,coverage,peak_comp,stack_comp,compression_time,peak_decomp,stack_decomp,decompression_time,compressed_size,plain_size"
GCIS_EXECUTABLE_PATH="../../GCIS/build/src/./gc-is-codec"

os_name=$(uname -s)
if [ "$os_name" = "Darwin" ]; then
    stat_options="-f %z"
else
    stat_options="-c %s"
fi

check_and_create_folder() {
    echo -e "\n\n${GREEN}%%% Creating directories for files in case don't exist ${RESET}."
    if [ ! -d "$PIZZA_DIR" ]; then
        mkdir -p "$PIZZA_DIR"
    fi
    if [ ! -d "$COMPRESSED_DIR" ]; then
        mkdir -p "$COMPRESSED_DIR"
    fi
    if [ ! -d "$COMPRESSED_DIR/$CURRENT_DATE" ]; then
        mkdir -p "$COMPRESSED_DIR/$CURRENT_DATE"
    fi
    if [ ! -d "$REPORT_DIR" ]; then
        mkdir -p "$REPORT_DIR"
    fi
    if [ ! -d "$REPORT_DIR/$CURRENT_DATE" ]; then
        mkdir -p "$REPORT_DIR/$CURRENT_DATE"
        mkdir -p "$COMPRESSED_DIR/$CURRENT_DATE/extract"
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

validate_compression_and_decompression() {
    if ! cmp -s "$1" "$2"; then
        echo "$1 and $2 are different." >> $GENERAL_REPORT
    fi 
}

gcis_generate_report() {
    CODEC=$1
    PLAIN=$2
    REPORT=$3
    FILE_NAME=$4
    SIZE_PLAIN=$5
    OUTPUT="$COMPRESSED_DIR/$CURRENT_DATE/$FILE_NAME"
	echo -n "$FILE_NAME-gcis-$CODEC,-," >> $report
    "$GCIS_EXECUTABLE_PATH" -c "$PLAIN" "$OUTPUT-gcis-$CODEC" "-$CODEC" "$REPORT"
	"$GCIS_EXECUTABLE_PATH" -d "$OUTPUT-gcis-$CODEC" "$OUTPUT-gcis-$CODEC-plain" "-$CODEC" "$REPORT"
	echo -n $(stat "$stat_options" "$OUTPUT-gcis-$CODEC")"," >> "$REPORT"
	echo  "$SIZE_PLAIN" >> $REPORT
}

dcx_generate_report() {
    echo -e "\n${GREEN}%%% REPORT: Compresses the files from pizza_chilli, decompresses them, and compares the result with the original version${RESET}."

    make clean -C ../compressor/
    make compile -C ../compressor/

    for plain_file in $FILE_PATHS; do
        IFS="/" read -ra file_name <<< "$plain_file"
        in_plain="$PIZZA_DIR/${file_name[1]}"
        report="$REPORT_DIR/$CURRENT_DATE/${file_name[1]}-dcx-encoding.csv"
        
        #create a file header
        echo $HEADER > $report; 
        size_plain=$(stat "$stat_options" "$in_plain")
        for coverage in "${COVERAGE_LIST[@]}"; do
            echo -e "\n${BLUE}####### FILE: ${file_name[1]}, COVERAGE: ${coverage} ${RESET}"
            out_compressed="$COMPRESSED_DIR/$CURRENT_DATE/${file_name[1]}-coverage$coverage"
            out_descompressed=$out_compressed-plain
            #adding file name and coverage to the report
            echo -n "${file_name[1]}," >> $report 
            echo -n "$coverage," >> $report
            #perform compress
            ../compressor/./main $in_plain $out_compressed c $coverage $report
            #perform decompress
            ../compressor/./main $out_compressed.dcx $out_descompressed  d $coverage $report
            validate_compression_and_decompression "$in_plain" "$out_descompressed"
            #adding file size information to the report
            echo -n $(stat "$stat_options"  "$out_compressed.dcx")"," >> $report
            echo  "$size_plain" >> $report
        done
	    #compresses and decompresses the file using GCIS, with the Elias-Fano and Simple8B codec.
        gcis_generate_report "ef" "$in_plain" "$report" "${file_name[1]}" "$size_plain"
        gcis_generate_report "s8b" "$in_plain" "$report" "${file_name[1]}" "$size_plain"
    done
    make clean -C ../compressor/
}

run_extract() {
    echo -e "\n${GREEN}%%% Running the extract - GCIS and DCX ${RESET}."
    SUBSTRING_SIZE=(10)
    #(1 10 100 1000 10000)

    make clean -C ../compressor/
    make compile -C ../compressor/
    
    for plain_file in $FILE_PATHS; do
        IFS="/" read -ra file_name <<< "$plain_file"
        in_plain="$PIZZA_DIR/${file_name[1]}"
        extract_dir="$REPORT_DIR/$CURRENT_DATE/extract/${file_name[1]}"
        report="$REPORT_DIR/$CURRENT_DATE/${file_name[1]}-dcx-extract.csv"

        #python3 ../../GCIS/scripts/generate_extract_input.py "$in_plain" "$extract_dir-input"

        echo "alg,substring_size,peak,stack,time" > $report;
        compressed_file="$COMPRESSED_DIR/$CURRENT_DATE/${file_name[1]}"
        for interval_size in "${SUBSTRING_SIZE[@]}"; do
            echo -n "GCIS-ef,$interval_size," >> $report
            "$GCIS_EXECUTABLE_PATH" -e "$compressed_file-gcis-ef" "$extract_dir-input.${interval_size}_query" -ef "$report"
            echo "" >> $report
            for coverage in "${COVERAGE_LIST[@]}"; do
                echo -e "\n${BLUE}####### FILE: ${file_name[1]}, COVERAGE: ${coverage} ${RESET}"
                echo -n "DC$coverage,$interval_size," >> $report
                #todo: não sobrescrever valores de extract
                ../compressor/./main "$compressed_file-coverage$coverage.dcx" "result_extract_temp.txt" e $coverage "$extract_dir-input.${interval_size}_query" "$report"
                echo "" >> $report
            done
        done
    done

}

python_setup_and_generate_graphs() {
    libraries="pandas matplotlib"
    for lib in $libraries; do
        if ! python3 -c "import $lib" &> /dev/null; then
            echo -e "\n${GREEN}%%%Installing $lib library${RESET}."
            pip3 install $lib
        fi
    done

    echo -e "\n${GREEN}%%% Generates comparison charts between GCIS and DCX.${RESET}\n"
    python3 report.py ../$report ../gcis_result.csv
}

if [ "$0" = "$BASH_SOURCE" ]; then
#    check_and_create_folder
#    download_files
#    dcx_generate_report
    run_extract
#   python_setup_and_generate_graphs
fi