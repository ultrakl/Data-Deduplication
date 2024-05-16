#include <iostream>
#include <string>
#include <map>
class Base{
public:
    explicit Base(int id, int ofset, int chunksz) 
    : id(id), offset(ofset), chunkSize(chunksz), iscompressed(false){}
    Base(const Base&) = default;
    Base(Base&&) = default;
    size_t size() const {return chunkSize;}
    size_t addr() const {return offset;}
    int num() const{return id;}
    void setdelta() {iscompressed = true;}
    bool isset() const {return iscompressed;}
private:
    int id;
    size_t offset;
    size_t chunkSize;
    bool iscompressed;
};
class Delta{
public:
    explicit Delta(int id, int ofset, int base_id, int base_index, std::string patch)
    : id(id), baseid(base_id), offset(ofset), base_index(base_index), patch(patch) {}
    Delta(const Delta&) = default;
    Delta(Delta&&) = default;
public:
    int num() const{return id;}
    int basenum() const{return baseid;}
    size_t baseAddr() const{return base_index;}
    size_t addr() const{return offset;}
    const std::string& delta() const{return patch;}
private:
    int id, baseid;
    size_t offset;
    size_t base_index;
    std::string patch;
};

#include <unordered_map>
std::unordered_map<std::string, Base> fp_to_base;
std::unordered_map<std::string, Delta> fp_to_delta;