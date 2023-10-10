#include "utils.hpp"
#include "compressor.hpp"
#include "uarray.h"
#include "stack_count.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <math.h>
#include <fstream>

using namespace std;
using namespace std::chrono;
using timer = std::chrono::high_resolution_clock;

#define ASCII_SIZE 255
#define GET_RULE_INDEX() (xs[i]-1)*coverage


void grammar(char *fileIn, char *fileOut, char *reportFile, char *queriesFile, char op, int coverage) {
    clock_t start, finish;
    double duration =0.0;
    i32 textSize;
    void* base = stack_count_clear();
    switch (op){
        case 'c': {
            vector<i32> header;
            //preparing text
            unsigned char *text;
            readPlainText(fileIn, text, textSize, coverage);
            i32* uText = (i32*)calloc(textSize,sizeof(i32));
            for(int i=0; i < textSize; i++)uText[i] = (i32)text[i];
            free(text);

            //starting process
            auto start = timer::now();
            i32 nTuples = textSize/coverage;
            i32 *tuples = (i32*) malloc(nTuples * sizeof(i32));
            compress(uText, tuples, textSize, strcat(fileOut,".dcx"), 0, coverage, header, ASCII_SIZE);
            auto stop = timer::now();
            duration = (double)duration_cast<seconds>(stop - start).count();

            //printing compressed informations
            #if SCREEN_OUTPUT==1
                cout << "\n\n\x1b[32m>>>> Encode <<<<\x1b[0m\n";
                i32 levels = header.at(0);
                grammarInfo(header.data(), levels, coverage);
            #endif

            free(uText);
            free(tuples);
            break;
        }
        case 'd': {
            unsigned char *leafLevelRules, *text;
            i32 *header = nullptr;
            uarray **encodedSymbols;

            //reading compressed text and rules per level
            i32 xsSize = 0;
            readCompressedFile(fileIn, header, encodedSymbols, xsSize, coverage, leafLevelRules);

            //starting process
            auto start = timer::now();
            decode(text, header, encodedSymbols, xsSize, leafLevelRules, coverage);
            auto stop = timer::now();
            duration = (double)duration_cast<seconds>(stop - start).count();

            //saving output
            saveDecodedText(fileOut, text, xsSize);

            //printing compressed informations
            #if SCREEN_OUTPUT==1
                cout << "\n\n\x1b[32m>>>> Decompression <<<<\x1b[0m\n";
                grammarInfo(header, header[0], coverage);
            #endif

            free(leafLevelRules);
            free(text);
            for(int i=0; i < header[0]; i++)ua_free(encodedSymbols[i]);
            free(encodedSymbols);
            free(header);
            break;
        }
        case 'e': {
            unsigned char *leafLevelRules, *text;
            i32 *subtree_size = nullptr, l, r, txtSize;
            uarray **encodedSymbols;
            int xsSize = 0, levels;
            vector<pair<i32, i32>> queries;

            //preparing data
            readCompressedFile(fileIn, subtree_size, encodedSymbols, xsSize, coverage, leafLevelRules);
            ifstream file(queriesFile);
            if (!file.is_open())error("Unable to open file with intervals");
            while (file >> l >> r) queries.push_back(make_pair(l, r));
            file.close();

            //start process
            levels=subtree_size[0];
            for(int i=0; i < levels; i++)subtree_size[i] = pow(coverage, levels-i);
            txtSize = queries[0].second-queries[0].first+1;
            text = (unsigned char*)calloc(txtSize+1, sizeof(unsigned char));
            duration = extract_batch(fileOut, text, subtree_size, encodedSymbols, leafLevelRules, coverage, txtSize, queries, levels);

            free(leafLevelRules);
            free(text);
            free(subtree_size);
            for(int i=0; i < levels; i++)ua_free(encodedSymbols[i]);
            free(encodedSymbols);
            break;
        }
        default: {
            cout << "\n>>> Invalid option! <<< \n"
                 << "\tPlease one of the options below:\n"
                 << "\tc - to compress the text;\n"
                 << "\td - to decompress the text.\n"
                 << "\te - to extract substring[l,r] from the text.\n";
            break;
        }
    }
    //generate reports
    #if REPORT == 1
        generateReport(reportFile, duration, base);
    #endif
    printf("Time: %5.15lf(s)\n",duration);
}

void readPlainText(char *fileName, unsigned char *&text, i32 &textSize, int coverage) {
    FILE*  file= fopen(fileName,"r");
    isFileOpen(file, "An error occurred while opening the input plain file");

    fseek(file, 0, SEEK_END);
    textSize = ftell(file);
    i32 i = textSize;
    int nSentries=padding(textSize, coverage);
    textSize += nSentries;

    text = (unsigned char*)malloc(textSize*sizeof(unsigned char));
    while(i < textSize) text[i++] =0;

    fseek(file, 0, SEEK_SET);
    fread(text, 1, textSize-nSentries, file);
    fclose(file);
}

void readCompressedFile(char *fileName, i32 *&header, uarray **&encodedSymbols, i32 &xsSize, int coverage, unsigned char *&leafLevelRules) {
    FILE*  file= fopen(fileName,"rb");
    isFileOpen(file, "An error occurred while opening the compressed file.");

    //get header
    i32 levels;
    fread(&levels, sizeof(i32), 1, file);
    header = (i32*)calloc(levels+1, sizeof(i32));
    header[0]=levels;
    fread(&header[1], sizeof(i32), levels, file);

    //get xs and internal nodes
    encodedSymbols = (uarray**)malloc(header[0]*sizeof(uarray));
    xsSize = header[1];
    for(i32 i=0; i < header[0]; i++) {
        if(i == 0)encodedSymbols[0] = ua_alloc(xsSize, ceil(log2(xsSize))+1);
        else encodedSymbols[i] = ua_alloc(header[i]*coverage, ceil(log2(header[i+1]))+1);
        /*cada V[i] armazena até 64b (n*b=número total de bits).*/
        fread(encodedSymbols[i]->V, sizeof(u64), (encodedSymbols[i]->n * encodedSymbols[i]->b/64)+1, file);
    }

    //get leaf nodes
    i32 size = header[header[0]]*coverage;
    leafLevelRules = (unsigned char*)malloc(size*sizeof(unsigned char));
    fread(&leafLevelRules[0], sizeof(unsigned char), size, file);

    fclose(file);
}

void grammarInfo(i32 *header, int levels, int coverage) {
    cout << "\tCompressed file information:\n" <<
            "\t\tSize of Tuples: " << coverage <<
            "\n\t\tAmount of levels: " << levels << endl;

    for(int i=levels; i >0; i--){
        printf("\t\tLevel: %d - amount of rules: %u.\n",i,header[i]);
    }
}

void compress(i32 *text, i32 *tuples, i32 textSize, char *fileName, int level, int coverage, vector<i32> &header, i32 sigma){
    i32 nTuples = textSize/coverage, qtyRules=0;
    i32 reducedSize =  nTuples + padding(nTuples, coverage);
    i32 *rank = (i32*) calloc(reducedSize, sizeof(i32));
    uarray *encdIntRules = nullptr;
    unsigned char *leafRules = nullptr;

    sigma+=coverage;
    radixSort(text, nTuples, tuples, sigma, coverage);
    createLexNames(text, tuples, rank, qtyRules, nTuples, coverage);
    header.insert(header.begin(), qtyRules);

    if(level !=0)selectUniqueRules(text, encdIntRules, tuples, rank, nTuples, coverage, level, qtyRules, sigma);
    else selectUniqueRules(text, leafRules, tuples, rank, nTuples, coverage, level, qtyRules);

    if(qtyRules < nTuples){
        compress(rank, tuples, reducedSize, fileName, level+1, coverage, header, qtyRules);
    }else {
        header.insert(header.begin(), level+1);
        storeStartSymbol(fileName, rank, header);
    }
    storeRules(fileName, encdIntRules, leafRules, level, qtyRules*coverage);

    if(encdIntRules != nullptr) ua_free(encdIntRules);
    else if(leafRules != nullptr) free(leafRules);
    free(rank);
}

void decode(unsigned char *&text, i32 *header, uarray **encodedSymbols, i32 &xsSize, unsigned char *leafLevelRules, int coverage) {
    i32 *xs = (i32*)calloc(xsSize, sizeof(i32));
    for(int i=0; i < xsSize; i++)xs[i] = (i32)ua_get(encodedSymbols[0], i);

    //decode internal nodes
    for(i32 i=1; i < header[0]; i++) {
        decodeSymbol(encodedSymbols[i], xs, xsSize, coverage);
    }

    //decode last level
    i32 size=xsSize*coverage, i=0;
    text = (unsigned char*)malloc(size*sizeof(unsigned char));
    for(i=0, size=0; i < xsSize; i++){
        if(xs[i] == 0)continue;
        for(int j=0; j < coverage; j++) {
            char ch = leafLevelRules[GET_RULE_INDEX()+j];
            if(ch != 0)text[size++] = ch;
        }
    }
    xsSize = size;
    free(xs);
}

double extract_batch(char *fileName, unsigned char *&text, int *subtree_size, uarray **encodedSymbols, unsigned char *leafLevelRules, int coverage, i32 txtSize, vector<pair<i32, i32>> queries, int levels) {
    i32 *temp = (i32*)malloc(50000*sizeof(i32));
    i32 *xs = (i32*)malloc(50000*sizeof(i32));

    duration<double> duration;
    auto first = timer::now();
    auto total_time = timer::now();
    for(auto i : queries) {
        auto t0 = timer::now();
        extract(text, temp, xs, subtree_size, encodedSymbols, leafLevelRules, coverage, txtSize, i.first, i.second, levels);
        auto t1 = timer::now();
        total_time += t1-t0;

        #if FILE_OUTPUT == 1
            FILE* fileOutput = fopen(fileName,"a");
            isFileOpen(fileOutput, "An error occurred while opening the compressed file.");
            fprintf(fileOutput, "[%d,%d]\n", i.first,i.second);
            fwrite(&text[0], sizeof(char), i.second-i.first+1, fileOutput);
            fprintf(fileOutput, "\n");
            fclose(fileOutput);
        #endif
    }
    duration = total_time - first;
    free(temp);
    free(xs);
    return duration.count();
}

void extract(unsigned char *&text, i32 *temp, i32 *xs, int *subtree_size, uarray **encodedSymbols, unsigned char *leafLevelRules, int coverage, i32 txtSize, i32 l, i32 r, int levels){
    //Determines the interval in Xs that we need to decode
    i32 start_node = l/subtree_size[0], end_node = r/subtree_size[0], size;
    i32 xsSize = end_node - start_node + 1;
    l = l%subtree_size[0], r = r - (start_node*subtree_size[0]);

    //get xs
    for(int i=start_node, j=0; j < xsSize; i++)xs[j++] = (i32)ua_get(encodedSymbols[0], i);

    for(int j=1; j < levels; j++) {
        for(int i =0,p=0; i < xsSize; i++) {
            if(xs[i] == 0)break;
            for(int k=0; k < coverage; k++) {
                temp[p++] = ua_get(encodedSymbols[j], GET_RULE_INDEX()+k);
            }
        }
        xsSize *=coverage;

        //Choose rules (nodes) that we will use in the next iteration
        start_node = l/subtree_size[j];
        end_node = r/subtree_size[j];
        size = end_node - start_node+1;

        //symbol to translate in the next level
        for(int m=0, k=start_node; m < size && k < xsSize; k++)xs[m++] = temp[k];

        l = l%subtree_size[j];
        r -= start_node*subtree_size[j];
    }

    int k= l % subtree_size[levels-1];
    char ch;
    for(int i=0,j=0; i < size && j < txtSize; i++){
        for(; k < coverage && j < txtSize; k++) {
            ch = leafLevelRules[GET_RULE_INDEX()+k];
            if(ch != 0)text[j++] = ch;
        }
        k=0;
    }
}

void storeStartSymbol(char *fileName, i32 *startSymbol, vector<i32> &header) {
    uarray *encodedSymbol = ua_alloc(header[1], ceil(log2(header[1]))+1);
    for(int i=0, j=0; i < header.at(1);i++){
        if(startSymbol[i] ==0)break;
        ua_put(encodedSymbol, j++, startSymbol[i]);
    }

    FILE* file = fopen(fileName,"wb");
    isFileOpen(file, "An error occurred while opening the file for store start symbol.");
    fwrite(&header[0], sizeof(i32), header.size(), file);
    size_t numElements = (encodedSymbol->n * encodedSymbol->b/64) + 1;
    fwrite(encodedSymbol->V, sizeof(u64), numElements, file);
    ua_free(encodedSymbol);
    fclose(file);
}

void selectUniqueRules(i32 *text, unsigned char *&rules, i32 *tuples, i32 *rank, i32 nTuples, int coverage, int level, i32 qtyRules) {
    i32 lastRank = 0, n=0;
    rules = (unsigned char*)malloc(qtyRules*coverage*sizeof(unsigned char));

    for(i32 i=0; i < nTuples; i++) {
        if(rank[tuples[i]/coverage] == lastRank)continue;
        lastRank = rank[tuples[i]/coverage];

        for(int k=0; k < coverage; k++){
            rules[n++] = (unsigned char)text[tuples[i]+k];
        }
    }
}

void selectUniqueRules(i32 *text, uarray *&rules, i32 *tuples, i32 *rank, i32 nTuples, int coverage, int level, i32 qtyRules, i32 sigma){
    i32 lastRank = 0, n=0;
    rules = ua_alloc(qtyRules*coverage, ceil(log2(sigma))+1);

    for(i32 i=0; i < nTuples; i++) {
        if(rank[tuples[i]/coverage] == lastRank)continue;
        lastRank = rank[tuples[i]/coverage];

        for(int k=0; k < coverage; k++){
            ua_put(rules, n++, text[tuples[i]+k]);
        }
    }
    rules->n = n;
}

void storeRules(char *fileName, uarray *encdIntRules, unsigned char *leafRules, int level, i32 size) {
    FILE*  file= fopen(fileName,"ab");
    isFileOpen(file, "An error occurred while opening the file for store rules.");

    if(level != 0 && encdIntRules != NULL){
        size_t numElements = (encdIntRules->n * encdIntRules->b/64) + 1;
        fwrite(encdIntRules->V, sizeof(u64), numElements, file);
    } else if(leafRules != NULL) {
        fwrite(&leafRules[0], sizeof(unsigned char), size, file);
    } else {
        error("Error while trying to store rules.");
    }

    fclose(file);
}

void decodeSymbol(uarray *encodedSymbols, i32 *&xs, i32 &xsSize, int coverage) {
    i32 *temp = (i32*)malloc(xsSize*coverage*sizeof(i32));//tirar mover

    for(int i =0,j=0; i < xsSize; i++) {
        if(xs[i] == 0)break;
        for(int k=0; k < coverage; k++) {
            temp[j++] = ua_get(encodedSymbols, GET_RULE_INDEX()+k);
        }
    }

    xsSize *=coverage;
    free(xs);//mover
    xs = (i32*)malloc(xsSize*sizeof(i32));
    for(int i=0,j=0; i < xsSize; i++)xs[j++] = temp[i];
    free(temp);//mover
}

void saveDecodedText(char *fileName, unsigned char* text, i32 size) {
    FILE*  file= fopen(fileName,"w");
    isFileOpen(file, "An error occurred while trying to open the file to save the decoded text." );
    fwrite(&text[0], sizeof(unsigned char), size, file);
    fclose(file);
}