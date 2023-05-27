#include "compressor-int.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <math.h>

using namespace std;

unsigned char *text;
vector<uint32_t> grammarInfo;

template <typename T>
void print(T v[], int n){
    cout << *(v);
    for(int i=1; i < n ; i++) cout  << ", " << *(v+i);
     cout << endl;
} 

void grammar(char *fileIn, char *fileOut, char op, int ruleSize) {
    long long int textSize;
    int module = ruleSize;
    switch (op){
        case 'e': {
            cout << "\n\n>>>> Encode <<<<\n";
            readPlainText(fileIn, textSize, module);
            uint32_t *uText = (uint32_t*)malloc(textSize*sizeof(uint32_t));
            if(uText == NULL)exit(EXIT_FAILURE);
            for(int i=0; i < textSize; i++)uText[i] = (uint32_t)text[i];
            encode(uText,textSize, fileOut, 0, module);

            int levels = grammarInfo.at(0);
            cout << "\tCompressed file information:\n" <<
                    "\t\tSize of Tuples: " << module <<
                    "\n\t\tAmount of levels: " << levels <<
                    "\n\t\tStart symbol size (including $): "<< grammarInfo.at(1) <<
                    endl;

            for(int i=grammarInfo.size()-1; i >0; i--){
                printf("\t\tLevel: %d - amount of rules: %u.\n",i,grammarInfo[i]);
            }
            free(uText);
            free(text);
            break;
        }
        case 'd': {
            unsigned char *rules0 = nullptr;
            uint32_t *textC =nullptr;
            cout << "\n\n>>>> Decode <<<<\n";
            readCompressedFile(fileIn, textC, textSize, rules0, module);
            uint32_t levels = grammarInfo.at(0);
            cout << "\tCompressed file information:\n" <<
                    "\t\tSize of Tuples: " << module <<
                    "\n\t\tAmount of levels: " << levels << endl;
            for(int i=grammarInfo.size()-1; i >0; i--){
                printf("\t\tLevel: %d - amount of rules: %u.\n",i,grammarInfo[i]);
            }
            decode(textC, textSize, levels-1, levels, fileOut, rules0, module);
            free(rules0);
            free(textC);
            break;
        }
        default: {
            cout << "\n>>> Invalid option! <<< \n"
                 << "\tPlease one of the options below:\n"
                 << "\te - to compress the text;\n"
                 << "\td - to decompress the text.\n";
            break;
        }
    }
}

void readPlainText(char *fileName, long long int &textSize, int module) {
    FILE*  file= fopen(fileName,"r");

    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    textSize = ftell(file);
    long long int i = textSize;
    int nSentries=calculatesNumberOfSentries(textSize, module);
    textSize += nSentries;
    text = (unsigned char*)malloc(textSize*sizeof(unsigned char));
    while(i < textSize) text[i++] =0;
    fseek(file, 0, SEEK_SET);
    fread(text, 1, textSize-nSentries, file);
    fclose(file);
}

void readCompressedFile(char *fileName, uint32_t *&textC, long long int &textSize, unsigned char* &rules0, int module) {
    int nRulesLastLevel;
    FILE*  file= fopen(fileName,"rb");

    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }

    uint32_t levels;
    fread(&levels, sizeof(uint32_t), 1, file);
    grammarInfo.push_back(levels);

    for(int i = levels-1; i >=0; i--){
        uint32_t n;
        fread(&n, sizeof(uint32_t), 1, file); 
        grammarInfo.push_back(n);
    }

    nRulesLastLevel = grammarInfo[levels];
    rules0 = (unsigned char*)malloc(nRulesLastLevel*module*sizeof(unsigned char));

    fseek(file, 0, SEEK_END);
    textSize = (((int)ftell(file) - (grammarInfo.size()*4) - (nRulesLastLevel*module))/4);
    textC = (uint32_t*)malloc(textSize*sizeof(uint32_t));

    fseek(file, grammarInfo.size()*sizeof(uint32_t), SEEK_SET);
    fread(textC, sizeof(uint32_t), textSize, file);

    fread(rules0, sizeof(char), nRulesLastLevel*module, file);
    fclose(file);
}

int calculatesNumberOfSentries(long long int textSize, int module) {
    if(textSize > module && textSize % module !=0) {
        return (ceil((double)textSize/module)*module) - textSize;
    }else if(textSize % module !=0){
        return module - textSize;
    }
    return 0;
}

void encode(uint32_t *uText, long long int textSize, char *fileName, int level, int module){
    long long int tupleIndexSize = textSize/module;
    uint32_t *rank = (uint32_t*) calloc(textSize, sizeof(uint32_t));
    uint32_t *tupleIndex = (uint32_t*) calloc(tupleIndexSize, sizeof(uint32_t));

    radixSort(uText, tupleIndexSize, tupleIndex, level, module);
    long int qtyRules = createLexNames(uText, tupleIndex, rank, tupleIndexSize, module);
    grammarInfo.insert(grammarInfo.begin(), qtyRules);

    long long int redTextSize =  tupleIndexSize + calculatesNumberOfSentries(tupleIndexSize, module);
    uint32_t *redText = (uint32_t*) calloc(redTextSize, sizeof(uint32_t));
    createReducedText(rank, redText, tupleIndexSize, textSize, redTextSize, module);
   
    if(qtyRules < tupleIndexSize){
        encode(redText, redTextSize, fileName, level+1, module);
    }
    else {
        grammarInfo.insert(grammarInfo.begin(), level+1);
        storeStartSymbol(fileName, redText, redTextSize);
    }
    
    if(level!=0)storeRules(uText, tupleIndex, rank, tupleIndexSize, fileName, module);
    else storeRules(tupleIndex, rank, tupleIndexSize, fileName, module);
    free(redText);
    free(rank);
    free(tupleIndex);
}

void decode(uint32_t *textC, long long int textSize, int level, int qtyLevels, char *fileName, unsigned char *rules0, int module){
    int startLevel = 0;
    long long int xsSize = grammarInfo.at(1);
    uint32_t *symbol = (uint32_t*)malloc(xsSize * sizeof(uint32_t));
    for(int i=0; i < xsSize; i++)
        symbol[i] = textC[i];
    int l=1;
    startLevel += xsSize; 
    while(level > 0 && l < qtyLevels) {
        decodeSymbol(textC,symbol, xsSize, level, startLevel, module);
        startLevel += (grammarInfo[l]*module);
        l++;
        level--;
    }

    saveDecodedText(symbol, xsSize, fileName, rules0, module);
    free(symbol);
}

void radixSort(uint32_t *uText, int tupleIndexSize, uint32_t *tupleIndex, long int level, int module){
    uint32_t *tupleIndexTemp = (uint32_t*) calloc(tupleIndexSize, sizeof(uint32_t));
    long int big=uText[0];

    for(int i=1; i < tupleIndexSize*module;i++)if(uText[i] > big)big=uText[i];
    for(int i=0, j=0; i < tupleIndexSize; i++, j+=module)tupleIndex[i] = j;
    //Tentar entender qual deve ser o tamanho do alfabeto, definir este tamanho como sendo o maior valor em ordem lexicográfica está dando erro, sendo preciso incrementar
    long int sigma = big+module;
    int *bucket =(int*) calloc(sigma, sizeof(int));

    for(int d= module-1; d >=0; d--) {
        for(int i=0; i < sigma;i++)bucket[i]=0;
        for(int i=0; i < tupleIndexSize; i++) bucket[uText[tupleIndex[i] + d]+1]++; 
        for(int i=1; i < sigma; i++) bucket[i] += bucket[i-1];

        for(int i=0; i < tupleIndexSize; i++) {
            int index = bucket[uText[tupleIndex[i] + d]]++;
            tupleIndexTemp[index] = tupleIndex[i];
        }
        for(int i=0; i < tupleIndexSize; i++) tupleIndex[i] = tupleIndexTemp[i];
    }

    free(bucket);
    free(tupleIndexTemp);
}

long int createLexNames(uint32_t *uText, uint32_t *tupleIndex, uint32_t *rank, long int tupleIndexSize, int module) {
    long int i=0;
    long int uniqueTriple = 1;
    rank[tupleIndex[i++]] = 1;
    for(; i < tupleIndexSize; i++) {
        bool equal = true;
        for(int j=0; j < module; j++)
            if(uText[tupleIndex[i-1]+j] != uText[tupleIndex[i]+j]){
                equal = false;
                break;
            }
        if(equal)rank[tupleIndex[i]] = rank[tupleIndex[i-1]];
        else {
            rank[tupleIndex[i]] = rank[tupleIndex[i-1]] + 1;
            uniqueTriple++;
        }
    }
   
    printf("Número de trincas = %ld, quantidade de trincas sem repetição: %ld, quantidade de trincas com repetição = %ld\n", tupleIndexSize, uniqueTriple, tupleIndexSize-uniqueTriple);
    return uniqueTriple;
}

void  createReducedText(uint32_t *rank, uint32_t *redText, long long int tupleIndexSize, long long int textSize, long long int redTextSize, int module) {
    for(int i=0, j=0; j < textSize; i++, j+=module) 
        redText[i] = rank[j];

    while(tupleIndexSize < redTextSize)
        redText[tupleIndexSize++] = 0;
}

void storeStartSymbol(char *fileName, uint32_t *startSymbol, int size) {
    FILE*  file= fopen(fileName,"wb");

    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }
    fwrite(&grammarInfo[0], sizeof(uint32_t), grammarInfo.size(), file);
    for(int i=0; i < size;i++){
        if(startSymbol[i]!=0)
            fwrite(&startSymbol[i], sizeof(uint32_t), 1, file);
    }
    fclose(file);
}

void storeRules(uint32_t *uText, uint32_t *tupleIndex, uint32_t *rank, int tupleIndexSize, char *fileName, int module){
    uint32_t lastRank = 0;

    FILE*  file= fopen(fileName,"ab");

    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }

    for(int i=0; i < tupleIndexSize; i++) {
        if(rank[tupleIndex[i]] == lastRank)
            continue;
        lastRank = rank[tupleIndex[i]];
        fwrite(&uText[tupleIndex[i]], sizeof(uint32_t), module, file);
    }
    fclose(file);
}

void storeRules(uint32_t *tupleIndex, uint32_t *rank, int tupleIndexSize, char *fileName, int module){
    int lastRank = 0;
    
    FILE*  file= fopen(fileName,"ab");

    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }

    for(int i=0; i < tupleIndexSize; i++) {
        if(rank[tupleIndex[i]] == lastRank)
            continue;
        lastRank = rank[tupleIndex[i]];
        fwrite(&text[tupleIndex[i]], sizeof(char), module, file);
    }
    fclose(file);
}

void decodeSymbol(uint32_t* textC, uint32_t *&symbol, long long int &xsSize, int l, int start, int module) {
    uint32_t *symbolTemp = (uint32_t*) malloc(xsSize*module* sizeof(uint32_t*));
    int j = 0;
    for(int i=0; i < xsSize; i++) {
        int rule = symbol[i];
        if(rule==0)continue; 
        int rightHand = start + ((rule-1)*module);
        //cout << "\n---- Level: " << l << endl;
        //cout << "\nv" << rule << " -> ";
        for(int k=0; k < module; k++){
            if(textC[rightHand+k] ==0)continue;
            symbolTemp[j++] = textC[rightHand+k];
            //printf("%d . ", symbolTemp[j-1]);
        }
    }

    free(symbol);

    xsSize = j;
    symbol = (uint32_t*) malloc(xsSize* sizeof(uint32_t*));
    for(int i=0; i < xsSize; i++) symbol[i] = symbolTemp[i];
    free(symbolTemp);
}

void saveDecodedText(uint32_t *symbol, long long int symbolSize, char *fileName, unsigned char* rules0, int module) {
    FILE*  file= fopen(fileName,"w");
    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }

    int textSize = symbolSize*module;
    char *str = (char*)malloc(textSize*sizeof(char));
    for(int i=0,k=0; i < symbolSize; i++){
        int rightHand = (symbol[i]-1)*module;
        for(int j=0; j < module; j++){
            if(rules0[rightHand+j]==0){
                textSize--;
                continue;
            }
            str[k++] = rules0[rightHand+j];
        }
    }
    fwrite(&str[0], sizeof(char), textSize, file);
    free(str);
    fclose(file);
}
