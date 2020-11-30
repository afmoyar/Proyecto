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

std::string getRandomKey(std::vector<std::string> pool, std::vector<int> &numbers, const int seed) {
    
    std::string key;
    srand( (unsigned) time(NULL) + getpid() * seed);
    int random_number = rand();
    int random_index = numbers[random_number % (numbers.size() - 1)];
    //std::cout<<"random index: "<<random_index<<std::endl;
    key = pool[random_index];
    //std::cout<<"antes"<<numbers.size()<<std::endl;
    numbers.erase(numbers.begin() + random_number % (numbers.size() - 1));
    //std::cout<<"despues"<<numbers.size()<<std::endl;
    return key;   
}

std::vector<std::string> generatePool(const int pool_size){

    std::vector<std::string> pool (pool_size);
    for (int i = 0; i < pool_size; i++)
    {
        pool[i] = generateKey(10, i);
    }
    return pool;
}

std::vector<std::vector<std::string>> assignKeysToNodes(std::vector<std::string> pool, const int pool_size, const int numNodes, int num_keys_node){

    
    std::vector<std::vector<std::string>> NodeKeys(numNodes);
    int seed = 0;
    for (int i = 0; i < numNodes; ++i)
    {
        std::vector<int> numbers (pool.size());
        for (size_t i = 0; i < numbers.size(); ++i) {
            numbers[i] = i;
        }

      NodeKeys[i] = std::vector<std::string> (num_keys_node);
      for (size_t j = 0; j < NodeKeys[i].size(); ++j)
      {
        NodeKeys[i][j] = getRandomKey(pool, numbers, seed++);
      }

    }
    return NodeKeys;
}





#endif
