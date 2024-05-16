#include <openssl/evp.h>
#include <sstream> 
#include <algorithm>
#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include <set>
#include <cstdint>
#include <iomanip>
#include <openssl/sha.h>

class RabinFingerprint {
private:
    uint32_t hash;
    uint32_t window_size;
    std::vector<uint8_t> buffer;

public:
    RabinFingerprint(uint32_t window_size) : hash(0), window_size(window_size) {
        buffer.reserve(window_size);
    }

    void clear() {
        hash = 0;
        buffer.clear();
    }

    void update(uint8_t byte) {
        if (buffer.size() >= window_size) {
            buffer.erase(buffer.begin());
        }
        buffer.push_back(byte);
        hash = (hash * 256 + byte) % 4294967291;  
    }

    uint32_t get_hash() {
        return hash;
    }
};

class N_Transform {
private:
    RabinFingerprint rabin;
    int num_sketch;
    int num_group;
    //系数数组
    std::vector<std::pair<uint32_t, uint32_t>> coefficients;

public:
    N_Transform(int num_sketch=12, int num_group=3, int sliding_window=48) 
        : rabin(RabinFingerprint(sliding_window)), num_sketch(num_sketch), num_group(num_group) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(1, 4294967295);

        for (int i = 0; i < num_sketch; ++i) {
            coefficients.push_back({dist(gen), dist(gen)});
        }
    }
    // 产生SF也是要通过连续集合中各个特征进行Rabin计算得到的
    uint32_t grouping(const std::vector<uint32_t>& feature_list) {
        rabin.clear();
        for (auto feature : feature_list) {
            for (int i = 0; i < 4; ++i) {
                rabin.update((feature >> (24 - 8 * i)) & 0xFF);
            }
        }
        return rabin.get_hash();
    }

    std::vector<uint32_t> build_sketch(const std::vector<uint8_t>& byte_data) {
        std::vector<uint32_t> features(num_sketch, 0); 
        for (auto ch : byte_data) {
            rabin.update(ch);
            uint32_t fingerprint = rabin.get_hash(); //逐字节计算
            
            for (size_t i = 0; i < coefficients.size(); ++i) {
                uint32_t linear_transform = (coefficients[i].first * fingerprint + coefficients[i].second) % 4294967295;
                if (features[i] <= linear_transform) {
                    features[i] = linear_transform;
                }
            }
        }

        assert(features.size() == num_sketch);
        
        // 分组特征
        std::vector<uint32_t> super_features;
        for (int i = 0; i < num_sketch; i += 4) {
            auto group = grouping(std::vector<uint32_t>(features.begin() + i, features.begin() + i + 4));
            super_features.push_back(group);
        }

        assert(super_features.size() == num_group);
        
        return super_features;
    }

};

class Finesse {
private:
    RabinFingerprint rabin;
    int num_sketch;
    int num_group;
    int per_group;

public:
    Finesse(int num_sketch=12, int num_group=4, int sliding_window=48) 
        : rabin(RabinFingerprint(sliding_window)), num_sketch(num_sketch), num_group(num_group) {
        per_group = num_sketch / num_group;
    }
    std::string sha3(const std::string input)
    {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(mdctx, EVP_sha3_256(), NULL);
        EVP_DigestUpdate(mdctx, input.c_str(), input.size());
        EVP_DigestFinal_ex(mdctx, hash, &hash_len);
        EVP_MD_CTX_free(mdctx);
        std::stringstream ss;
        for(int i = 0; i < hash_len; i++)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    std::vector<uint32_t> build_sketch(const std::vector<uint8_t>& byte_data) {
        std::vector<uint32_t> rabins;
    
        // 将块分割为子块
        std::vector<std::vector<uint8_t>> sub_chunks = divide_data(byte_data, num_sketch);
        
        for (auto& sub_chunk : sub_chunks) {
            uint32_t max_hash = 0;
            for (auto& ch : sub_chunk) {
                rabin.update(ch);
                uint32_t cur_hash = rabin.get_hash();
                max_hash = std::max(cur_hash, max_hash);
            }
            rabins.push_back(max_hash);
            rabin.clear();
        }
        assert(rabins.size() == num_sketch);
        
        std::vector<std::vector<uint32_t>> groups;
        for (size_t i = 0; i < rabins.size(); i += num_group) {
            std::vector<uint32_t> group;
            for (size_t j = 0; j < num_group; ++j) {
                group.push_back(rabins[i + j]);
            }
            std::sort(group.begin(), group.end());
            groups.push_back(group);
        }
        std::vector<uint32_t> hashes;
        for (int i = 0; i < per_group; ++i) {
            std::stringstream ss;
            for (auto& group : groups) {
                ss << std::setw(8) << std::setfill('0') << std::hex << group[i];
            }
            hashes.push_back(std::stoul(sha3(ss.str()).substr(0, 8), 0, 16));
        }

        return hashes;
    }

    std::vector<std::vector<uint8_t>> divide_data(const std::vector<uint8_t>& data, int n) {
        int length = data.size();
        int size = length / n ;
        int remainder = length % n;
        std::vector<int> sizes(n, size);
        for (int i = 0; i < remainder; ++i) {
            sizes[i]++;
        }
        std::vector<std::vector<uint8_t>> chunks;
        int start = 0;
        for (int size : sizes) {
            chunks.push_back(std::vector<uint8_t>(data.begin() + start, data.begin() + start + size));
            start += size;
        }
        return chunks;
    }
    
    /* std::string md5_hash(const std::string& values) {
        // This is a placeholder for MD5 hashing function
        return "md5_hash_placeholder";
    } */
};

bool resemble(const std::vector<uint32_t>& ref_sketch, const std::vector<uint32_t>& inc_sketch) {
    std::set<uint32_t> ref_set(ref_sketch.begin(), ref_sketch.end());
    std::set<uint32_t> inc_set(inc_sketch.begin(), inc_sketch.end());
    
    std::vector<uint32_t> intersection;
    std::set_intersection(ref_set.begin(), ref_set.end(), inc_set.begin(), inc_set.end(), std::back_inserter(intersection));
    
    return intersection.size() > 0;
}

/* 
int main() {
    N_Transform transform(12, 3, 48);
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto sketch = transform.build_sketch(data);
    for (auto feature : sketch) {
        std::cout << feature << " ";
    }
    std::cout << std::endl;

    return 0;
} */

/* int main() {
    Finesse finesse(12, 3, 48);
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto sketch = finesse.build_sketch(data);
    for (auto s : sketch) {
        std::cout << s << " ";
    }
    std::cout << std::endl;
    return 0;
} */