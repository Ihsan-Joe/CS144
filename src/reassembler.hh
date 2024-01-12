#pragma once

#include "byte_stream.hh"

#include <cstdint>
#include <forward_list>
#include <string>
#include <string_view>

class Reassembler
{
private:
    struct m_data_struct
    {
        uint64_t index;
        std::string data_string;
    };
    std::forward_list<m_data_struct> m_reassemble_buf{};
    m_data_struct m_buf_first_data{};
    uint64_t m_buf_use_capacity{0};
    uint64_t m_next_index{0};
    uint64_t m_pre_index{0};
    uint64_t m_pre_data_size{0};
    bool m_write_close{false};
    
    uint64_t m_func_calld{0};

    enum Classify_Returns
    {
        Beyond_Available_Capacity = 0,
        Direct_Push,
        Need_Cut_And_Push,
        Save_The_Buffer,
        Overlap
    };

private:
    Classify_Returns classify(uint64_t first_index, const std::string_view &data, uint64_t available_capacity) const;

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
