#pragma once

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <unordered_map>

class Reassembler
{
private:
    int count_called{1};
    uint64_t m_pre_index{0};
    uint64_t m_pre_capacity{0}; // 上一次insert的时候的capacity
    std::string m_pre_data{};
    uint64_t m_the_last_index{0}; // 存放最后一个index
    uint64_t m_next_currect_index{0};
    uint64_t m_rasmblr_buf_cur_data_num{0};
    std::unordered_map<uint64_t, std::string> m_rasmblr_buffer{};

private:
    static size_t handle_data_string(std::string &src, std::string &dec);

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
