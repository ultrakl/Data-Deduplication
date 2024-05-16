/* // extern "C" {
#include "../xdelta3.h"
// }
#include <iostream>
#include <string>
#include <vector>



//两个数据块之间的增量压缩
std::string generate_patch(std::vector<uint8_t> old_str, std::vector<uint8_t> new_str) {
    int old_size = old_str.size();
    int new_size = new_str.size();
    uint32_t outputsize;
    uint8_t* output = new uint8_t(new_size);
    // std::cout<<"what"<<std::endl;
    int ret = xd3_encode_memory(new_str.data(), new_size, old_str.data(), old_size, output, \
    (usize_t* )&outputsize, new_size, 9 << 20);
    if(ret == 0) {
        return std::string((char*)output, outputsize);
    }
    //如果返回值不为0，很大可能是因为两个块的长度过小或者相似程度过高导致无法进行增量压缩
    std::cout<<"haha?"<<std::endl;
    free(output);
    return "";
}
 */
#include "../xdelta3.h"
/* 
std::string generate_patch(const char* old_str, const char* new_str, int old_size, int new_size) {
    // int old_size = strlen(old_str);
    // int new_size = strlen(new_str);
    int outputsize;
    uint8_t* output = new uint8_t(new_size);
    // Allocate memory for delta encoding
    // xd3_avail_output avail_output = {delta_buf, 0, buf_size};

    // Set up the stream for delta encoding
    // xd3_set_source(&stream, &source);
    // xd3_avail_input avail_input = {(uint8_t*)new_str, (usize_t)new_size, 0};

    // 使用adler32进行fp计算并生成增量编码
    int ret = xd3_encode_memory((uint8_t* )new_str, new_size, (uint8_t* )old_str, old_size, output, (usize_t* )&outputsize, new_size,  6 << 20);
    // std::cout<<"fuck"<<' '<<ret<<std::endl;
    if(ret == 0) {
        return std::string(reinterpret_cast<char*>(output), outputsize);
    }
    free(output);
    return "";
} */


std::string generate_patch(std::vector<uint8_t> old_str, std::vector<uint8_t> new_str) {
    size_t old_size = old_str.size();
    size_t new_size = new_str.size();
    uint32_t outputsize;
    uint8_t* output = (uint8_t*)malloc(new_size);
    // std::cout<<"what"<<std::endl;
    int ret = xd3_encode_memory(new_str.data(), new_size, old_str.data(), old_size, output, \
    (usize_t* )&outputsize, new_size, 9 << 20);
    if(ret == 0) {
        return std::string((char*)output, outputsize);
    }
    //如果返回值不为0，很大可能是因为两个块的长度过小或者相似程度过高导致无法进行增量压缩
    // std::cout<<"haha?"<<std::endl;
    free(output);
    return "";
}