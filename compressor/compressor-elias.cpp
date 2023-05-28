#include "compressor-int.hpp"
#include "compressor-elias.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <math.h>
#include <sdsl/int_vector.hpp>
#include <sdsl/coder.hpp>

using namespace std;
using namespace sdsl;

vector<uint32_t> grammarInfo;
unsigned char *text;

template <typename T>
void print(T v[], int n){
    cout << *(v);
    for(int i=1; i < n ; i++) cout  << ", " << *(v+i);
     cout << endl;
} 

void grammar(char *fileIn, char *fileOut, char op, int ruleSize) {
    long long int textSize;
    int module = ruleSize;
    int levels=0;

    switch (op){
        case 'e': {
            cout << "\n\n>>>> Encode with Elias Gamma <<<<\n";

            readPlainText(fileIn, textSize, module);

            uint32_t *uText = (uint32_t*)malloc(textSize*sizeof(uint32_t));
            if(uText == NULL)
                exit(EXIT_FAILURE);
            for(int i=0; i < textSize; i++)uText[i] = (uint32_t)text[i];

            encode(uText,textSize, 0, module);

            levels = grammarInfo.at(0);
            cout << "\tCompressed file information:\n" <<
                    "\t\tAmount of levels: " << levels <<
                    "\n\t\tStart symbol size (including $): "<< grammarInfo.at(1) <<
                    endl;

            encodeTextWithEliasAndSave(fileOut);
            for(int i=levels; i >0; i--){
                printf("\t\tLevel: %d - amount of rules: %u.\n",i,grammarInfo[i]);
            }

            free(uText);
            free(text);
            break;
        }
        case 'd': {
            int_vector<32> decoded;

            cout << "\n\n>>>> Decode with Elias Gamma <<<<\n";
            readCompressedFile(fileIn, decoded, levels);

            cout << "\tCompressed file information:\n" <<
                    "\n\t\tAmount of levels: " << levels << endl;
            for(int i=levels; i >0; i--){
                printf("\t\tLevel: %d - amount of rules: %u.\n",i,decoded[i]);
            }
            decode(decoded, levels-1, levels, module, fileOut);
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

void readCompressedFile(char *fileName, int_vector<32> &decoded, int &levels) {
    coder::elias_gamma eg;
    int_vector<32> encoded;
    
    load_from_file(encoded, fileName);
    eg.decode(encoded, decoded);

    levels = decoded[0];
}

int calculatesNumberOfSentries(long long int textSize, int module) {
    if(textSize > module && textSize % module !=0) {
        return (ceil((double)textSize/module)*module) - textSize;
    }else if(textSize % module !=0){
        return module - textSize;
    }
    return 0;
}

void encode(uint32_t *uText, long long int textSize, int level, int module){
    long long int tupleIndexSize = textSize/module;
    uint32_t *rank = (uint32_t*) calloc(textSize, sizeof(uint32_t));
    uint32_t *tupleIndex = (uint32_t*) calloc(tupleIndexSize, sizeof(uint32_t));

    radixSort(uText, tupleIndexSize, tupleIndex, level, module);
    long int qtyRules = createLexNames(uText, tupleIndex, rank, tupleIndexSize, module);
    grammarInfo.insert(grammarInfo.begin(), qtyRules);

    long long int redTextSize =  tupleIndexSize + calculatesNumberOfSentries(tupleIndexSize, module);
    uint32_t *redText = (uint32_t*) calloc(redTextSize, sizeof(uint32_t));
    createReducedText(rank, redText, tupleIndexSize, textSize, redTextSize, module);
   
    if(qtyRules < tupleIndexSize)
        encode(redText, redTextSize, level+1, module);
    else {
        grammarInfo.insert(grammarInfo.begin(), level+1);
        for(int i=0; i < redTextSize;i++)
            if(redText[i]!=0)grammarInfo.push_back(redText[i]);
    }
    
    uint32_t lastRank = 0;
    for(int i=0; i < tupleIndexSize; i++) {
        if(rank[tupleIndex[i]] == lastRank)
            continue;
        lastRank = rank[tupleIndex[i]];
        for(int j=0; j < module;j++){
            if(level!=0)grammarInfo.push_back(uText[tupleIndex[i]+j]);
            else grammarInfo.push_back(text[tupleIndex[i]+j]);
        }
    }

    free(redText);
    free(rank);
    free(tupleIndex);
}

void decode(int_vector<32> decoded, int level, int qtyLevels, int module, char *fileName){
    int startRules = qtyLevels + decoded[1]+1;

    long long int xsSize = decoded[1];
    uint32_t *symbol = (uint32_t*)malloc(xsSize * sizeof(uint32_t*));
    for(int i=qtyLevels+1, j=0; j < xsSize; i++,j++)symbol[j] = decoded[i];

    int l=1;
    while(level > 0 && l < qtyLevels) {
        decodeSymbol(decoded,symbol, xsSize, level, startRules, module);
        startRules += (decoded[l]*module);
        l++;
        level--;
    }

    saveDecodedText(decoded, symbol, xsSize, module, fileName);
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

void encodeTextWithEliasAndSave(char *fileName){
    coder::elias_gamma eg;
    int_vector<32> v;
    int_vector<32> encoded;
    
    v.resize(grammarInfo.size());
    for(int i=0; i < grammarInfo.size();i++)v[i] = grammarInfo[i];
    
    eg.encode(v, encoded);
    store_to_file(encoded, fileName);
}


void decodeSymbol(int_vector<32> decoded, uint32_t *&symbol, long long int &xsSize, int level, int startRules, int module) {
    uint32_t *symbolTemp = (uint32_t*) malloc(xsSize*module* sizeof(uint32_t*));
    int j = 0;
    for(int i=0; i < xsSize; i++) {
        int rule = symbol[i];
        if(rule==0)continue; 
        int rightHand = startRules + ((rule-1)*3);
        //cout << "\n---- Level: " << l << endl;
        //cout << "The rule " << rule << " starts at " << rightHand << endl;
        //cout << "\nv" << rule << " -> ";
        for(int k=0; k < 3; k++){
            if(decoded[rightHand+k] ==0)continue;
            symbolTemp[j++] = decoded[rightHand+k];
            //if(isalpha(symbolTemp[j-1]))printf("%c . ", symbolTemp[j-1]);
            //else printf("%d . ", symbolTemp[j-1]);
        }
    }

    free(symbol);

    xsSize = j;
    symbol = (uint32_t*) malloc(xsSize* sizeof(uint32_t*));
    for(int i=0; i < xsSize; i++) symbol[i] = symbolTemp[i];
    free(symbolTemp);
}

void saveDecodedText(int_vector<32> decoded, uint32_t *symbol, int xsSize, int module, char *fileName){
    FILE*  file= fopen(fileName,"w");
    if(file == NULL) {
        cout << "An error occurred while opening the file" << endl;
        exit(EXIT_FAILURE);
    }
    
    int startLastLevel = decoded[0] + decoded[1]+1;
    for(int i=1; i < decoded[0]; i++) startLastLevel+= decoded[i]*module;

    int n = xsSize * module;
    char *text = (char*)malloc(n*sizeof(char));

    for(int i=0,k=0; i < xsSize; i++){
        int rightHand = startLastLevel + ((symbol[i]-1)*module);
        for(int j=0; j < module; j++){
            if(decoded[rightHand+j]==0){
                n--;
                continue;
            }
            text[k++] = decoded[rightHand+j];
        }
    }
    fwrite(&text[0], sizeof(char), n, file);
    free(text);
    fclose(file);
}
