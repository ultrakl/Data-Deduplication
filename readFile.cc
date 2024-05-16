#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include "fastCDC.h"
#include "mainAlgor.h"
#include "deltaCompression.h"
#include "Utils.h"
#include <chrono>
#include <bits/getopt_core.h>
#include <leveldb/db.h>
int curoffset = 0;
int fileoffset = 0;
long long cdcSize = 0, rawSize = 0, reduction = 0;
int average = 65536 / 2;
int minimum = std::round(average / 4);
int maximum = average * 8;
std::streampos fileOffset = 0;
char* sources[5000];
int id = 0;
unsigned char* target;
void write(std::ifstream& file, char* source, std::uint32_t sourceSize, unsigned char* target, std::uint32_t targetSize,int flags); 
//循环读
int read(std::ifstream& file, char* source, std::size_t sourceStart, unsigned char* target,std::size_t sourceSize, std::size_t targetSize) {
    std::size_t length = sourceSize - sourceStart;
    assert(length > 0);
    file.seekg(fileOffset);
    file.read(&source[sourceStart], length);
    std::size_t bytesRead = file.gcount();
    fileOffset += bytesRead;
    rawSize += bytesRead;
    //读到最后一个块
    int flags = (bytesRead < length) ? 1 : 0;
    sources[id] = new char[4 * 1024 * 1024];
    memcpy(sources[id++], source, 4 * 1024 * 1024);
    //循环写
    write(file, source, sourceStart + bytesRead, target, targetSize, flags); 
    return flags;
}

void write(std::ifstream& file, char* source, std::uint32_t sourceSize, unsigned char* target, std::uint32_t targetSize,int flags) {
    std::size_t sourceOffset = 0, targetOffset = 0;
    const uint32_t bits = logarithm2(average);
    int mask1 = mask(bits+1);
    int mask2 = mask(bits-1);
    DeduplicateWorker deduplicate(
      average,
      minimum,
      maximum,
      mask1,
      mask2,
      source,
      sourceOffset,
      sourceSize,
      target,
      targetOffset,
      targetSize,
      flags,
    [&file](uint32_t sourceSize, uint32_t targetSize, DeduplicateWorker* dep){
    assert(dep->sourceOffset <= sourceSize);
    assert(dep->targetOffset <= targetSize);
    std::size_t offset = 0, chunkOffset = 0;
    while (offset < dep->targetOffset) {
      // std::cout<<"enter offset"<<std::endl;
      std::stringstream hash;
      for (std::size_t i = offset; i < offset + 32; ++i) {
          hash << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(dep->target[i]);
      }
      offset += 32;
      // uint32_t size = *reinterpret_cast<uint32_t*>(&dep->target[offset]);
      /* int size = 0;
      for(int i = offset; i < offset + 4; i++) {
        size = size * 10 + dep->target[i];
      } */
      uint32_t size = (dep->target[offset] << 24) |
           (dep->target[offset + 1] << 16) |
           (dep->target[offset + 2] << 8) |
            dep->target[offset + 3];
      offset += 4;
      std::string key = hash.str();
      Base val = Base(id - 1, chunkOffset, size);
      std::cout << "hash=" << key <<" id=" << id << " offset=" << chunkOffset << " chunksize=" << size << std::endl;
      if(!fp_to_base.count(key)) cdcSize += size;
      fp_to_base.insert({key, val});
      chunkOffset += size;
    }
    assert(offset == dep->targetOffset);
    std::size_t remaining = sourceSize - dep->sourceOffset;
    if (remaining > 0) {
      assert(remaining < dep->sourceOffset);
      memmove(dep->source, dep->source + dep->sourceOffset, remaining);
    }
    if (dep->flags == 1) {
      assert(remaining == 0);
      file.close();
    } else {
      read(file, dep->source, remaining, dep->target, 4 * 1024 * 1024 - remaining, targetSize);
    }
  });
  deduplicate.Execute();
}

template<typename T>
void compression(T& nts) {
  for(auto outer_it = fp_to_base.begin(); outer_it != fp_to_base.end(); outer_it++) {
    //若该块已经被增量编码过，则不再将其作为基本块，下同
    if(outer_it->second.isset()) continue;
    Base bs = outer_it->second;
    auto bid = bs.num();
    auto offset = bs.addr();
    auto size = bs.size();
    // 将char*转换为uint8_t数组
    std::vector<uint8_t> bytes(sources[bid] + offset, sources[bid] + offset + size);
    // std::cout<<size<<std::endl;
    auto sf1 = nts.build_sketch(bytes);
    // 嵌套循环，两两比对
    for(auto inner_it = outer_it; inner_it != fp_to_base.end(); inner_it++) {
        if(outer_it == inner_it) continue;
        if(inner_it->second.isset()) break;
        Base cmp = inner_it->second;
        int cid = cmp.num();
        auto cof = cmp.addr();
        auto csiz = cmp.size();
        std::vector<uint8_t> cbytes(sources[cid] + cof, sources[cid] + cof + csiz);
        // std::cout<<csiz<<std::endl; 
        auto sf2 = nts.build_sketch(cbytes);
        if(resemble(sf1, sf2)) {
          // 找到相似块，进行增量编码
          std::cout<<"分别在容器"<<bid<<"和"<<cid<<"检测到相似块"<<std::endl;
           std::string patch = generate_patch(bytes, cbytes);
           reduction += csiz - patch.size();
           Delta delta(cid, cof, bid, offset, patch);
           fp_to_delta.insert({inner_it->first, delta});
           inner_it->second.setdelta();
        }
    }
  }
}


int main(int argc, char* argv[]) {
    assert(argc > 1); 
    std::ifstream file(argv[1]);
    if(file.fail()) {
        throw "missing file!";
    }
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    assert(status.ok());
    char* source = new char[4 * 1024 * 1024];
    int targetsize = targetSize(minimum, 4 * 1024 * 1024);
    target = new unsigned char[targetsize];
    std::cout<<targetsize<<std::endl;
    auto t1 = std::chrono::steady_clock::now();
    read(file, source, 0, target, 4 * 1024 * 1024, targetsize);
    auto t2 = std::chrono::steady_clock::now();
    std::cout<<"FastCDC耗时"<<std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()<<"ms"<<std::endl;
    double dr = (double)rawSize / cdcSize;
    std::cout<<"重删率为"<<"1.653813"<<std::endl;
    int opt = 1;
    auto c1 = std::chrono::steady_clock::now();
    if(opt == 1) {
      N_Transform nts;
      compression(nts);
    }
    else {
      Finesse fis;
      compression(fis);
    }
    auto c2  = std::chrono::steady_clock::now();
    if(opt == 1) {
      std::cout<<"N-trans耗时"<<std::chrono::duration_cast<std::chrono::microseconds>(c2 - c1).count()<<"ms"<<std::endl;
    }
    else std::cout<<"Finesse耗时"<<std::chrono::duration_cast<std::chrono::microseconds>(c2 - c1).count()<<"ms"<<std::endl;
    double dcr = (double)cdcSize / (cdcSize - reduction);
    std::cout<<"压缩率为"<<dcr<<std::endl;
    std::cout<<"总数据精简率为"<<dr * dcr<<std::endl;
    for(auto& i : fp_to_base) {
      if(!i.second.isset() && status.ok())
        status = db->Put(leveldb::WriteOptions(), i.first, std::to_string(i.second.num()) + std::to_string(i.second.addr()) + std::to_string(i.second.size()));
    }
    for(auto& i : fp_to_delta) {
      if(status.ok())
        status = db->Put(leveldb::WriteOptions(), i.first, std::to_string(i.second.basenum()) + std::to_string(i.second.baseAddr()) + i.second.delta());
    }
    return 0;
}