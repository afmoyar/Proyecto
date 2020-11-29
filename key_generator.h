#ifndef KEY_GEN_KEY_GEN_HEADER_H
#define KEY_GEN_KEY_GEN_HEADER_H
#include <iostream>
#include <ctime>
#include <unistd.h>


std::string generateKey(const int len, const int seed) {
    
    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    srand( (unsigned) time(NULL) * getpid() + seed);

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) 
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    
    
    return tmp_s;   
}

std::vector<std::string> generatePool(const int pool_size){

    std::vector<std::string> pool (pool_size);
    for (int i = 0; i < pool_size; i++)
    {
        pool[i] = generateKey(10, i);
    }
    return pool;
}


#endif
