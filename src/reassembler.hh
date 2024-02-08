#pragma once

#include "byte_stream.hh"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

class Reassembler
{
private:
    std::vector<char> m_buffer{};
    std::vector<bool> m_bool_buffer{};
    bool m_is_recive_FIN{false};

    void save_the_buffer(const std::string &data, uint64_t begin_index);
    void push(Writer &output, uint64_t first_unassemble);

private:
    // 其他成员变量声明...
    std::chrono::steady_clock::time_point start_time{}; // 计时器开始时间点

    // 开始计时
    void start_timer() {
        start_time = std::chrono::steady_clock::now();
    }

    // 结束计时，并返回经过的时间（毫秒）
    double end_timer() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        return duration.count(); // 返回执行时间，单位为毫秒
    }

    // 输出执行时间
    void print_execution_time(double milliseconds, const std::string &message) {
        std::cerr << message << "Execution time: " << milliseconds << " ms" << std::endl;
    }

    // 其他私有成员函数...


public:
    /*
     * Insert a new substring to be reassembled into a ByteStream.
     *   `first_index`: the index of the first byte of the substring
     *   `data`: the substring itself
     *   `is_last_substring`: this substring represents the end of the stream
     *   `output`: a mutable reference to the Writer
     *
     * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
     * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
     * learns the next byte in the stream, it should write it to the output.
     *
     * If the Reassembler learns about bytes that fit within the stream's available capacity
     * but can't yet be written (because earlier bytes remain unknown), it should store them
     * internally until the gaps are filled in.
     *
     * The Reassembler should discard any bytes that lie beyond the stream's available capacity
     * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
     *
     * The Reassembler should close the stream after writing the last byte.
     */
    void insert(uint64_t first_index, std::string data, bool is_last_substring, Writer &output);

    // How many bytes are stored in the Reassembler itself?
    uint64_t bytes_pending() const;
};
