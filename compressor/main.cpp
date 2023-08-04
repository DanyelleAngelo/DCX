#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "compressor-int.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    if(argc < 6) {
        cout << "\n\x1b[31m[ERROR]\x1b[0m  Number of invalid arguments! \n" <<
                "\tList of arguments:\n" <<
                "\t\tInput file  for encode or decode\n" <<
                "\t\tOutput file, contains the result of the chosen operation)\n" <<
                "\t\tOperation - for encode choose: \"e\", for decode choose: \"d\" \n" <<
                "\t\tSize of rules\n" << 
                "\t\tType of encode - options: int\n" << endl;
        exit(EXIT_FAILURE);
    }
    clock_t start, finish;

    char op = argv[3][0];
    int ruleSize = atoi(&argv[4][0]);

    if(strcmp(argv[5], "int") == 0) {
        start = clock();
        grammarInteger(argv[1], argv[2], op, ruleSize);
        finish = clock();
    }else {
        cout << "\x1b[35m\n\tNo other grammar options available. Only integer encoding offered!\n\x1b[0m" << endl;
        exit(0);
    }

    double duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("%5.2lf",duration);
}