#pragma once

#include "BPlusTree.h"

class API;

class IndexManager {
private:
    typedef std::map<std::string, BPlusTree<int> *> intMap;
    typedef std::map<std::string, BPlusTree<std::string> *> stringMap;
    typedef std::map<std::string, BPlusTree<float> *> floatMap;

    int static const TYPE_FLOAT = Attribute::TYPE_FLOAT;
    int static const TYPE_INT = Attribute::TYPE_INT;
    // other values mean the size of the char.Eg, 4 means char(4);

    API *api; // to call the functions of API

    intMap indexIntMap;
    stringMap indexStringMap;
    floatMap indexFloatMap;
    struct keyTmp {
        int intTmp;
        float floatTmp;
        std::string stringTmp;
    }; // the struct to help to convert the inputed string to specfied type
    struct keyTmp kt;

    int getDegree(int type);

    int getKeySize(int type);

    void setKey(int type, const std::string& key);


public:
    IndexManager(API *api);
    ~IndexManager();

    void createIndex(const std::string& filePath, int type);

    void dropIndex(const std::string& filePath, int type);

    offsetNumber searchIndex(const std::string& filePath, const std::string& key, int type);

    void insertIndex(const std::string& filePath, const std::string& key, offsetNumber blockOffset, int type);

    void deleteIndexByKey(const std::string& filePath, const std::string& key, int type);
};
