#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity){ }

size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    len = min(_capacity - _buffer.size(), len);
    for (size_t i = 0; i < len; ++i) {
        _buffer.push_back(data[i]);
        _write_cnt++;
    }
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res = "";
    size_t buffer_len = _buffer.size();
    size_t res_len = min(buffer_len, len);
    size_t i = 0;
    for (auto it = _buffer.begin(); (it != _buffer.end()) && (i < res_len); ++i, ++it) {
        res.push_back(*it);
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t buffer_len = _buffer.size();
    size_t res_len = min(buffer_len, len);
    for (size_t i = 0; i < res_len; i++) {
        _buffer.pop_front();
        _read_cnt++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const auto res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { _is_end = true;}

bool ByteStream::input_ended() const { return _is_end; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_cnt; }

size_t ByteStream::bytes_read() const { return _read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
