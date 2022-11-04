#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_appear_sign = true;
        _eof_index = index + data.length();
    }

    if (index >= _capacity + _expect_index) { return ;}
    if (index <= _expect_index && index + data.length() >= _expect_index) {
        insert_pair(data, index);
        write_substring();
    }
    else if (index > _expect_index) {
        insert_pair(data, index);
    }
}

void StreamReassembler::write_substring(){

    auto it = _map.begin();
    //iterate the _map and write useful data into _output_buffer
    for(; it != _map.end() ; it++){
        size_t data_len = it -> second.length();
        if(it->first <= _expect_index){
            if((data_len + it->first) <= _expect_index){
                continue;
            }
            size_t writed_len = it->first + data_len - _expect_index;
            /*这里_output.wirte()是lab0实现的函数,函数返回成功写入字符的数量*/
            _expect_index += _output.write(it->second.substr(_expect_index - it->first,writed_len));
        }
        else break;
    }
    /*删除已经处理过的字符流*/
    _map.erase(_map.begin(),it);
    /*如果已经处理好最后一个流了就设置eof*/
    if(_eof_appear_sign && _eof_index <= _expect_index){
        _output.end_input();
    }
}

void StreamReassembler::insert_pair(const std::string &data, const size_t index){
    size_t data_len = data.length();
    if (_map[index].length() < data_len) {
        _map[index] = data;
    }
}

//void StreamReassembler::merge_block_node(StreamReassembler::block_node &elem1, const StreamReassembler::block_node &elem2) {
//    block_node low, high;
//    if (elem1.index <= elem2.index) {
//        low = elem1;
//        high = elem2;
//    } else {
//        low = elem2;
//        high = elem1;
//    }
//    if (low.index + low.data.length() <= high.index) {
//        elem1.index = low.index;
//        elem1.data = low.data + high.data.substr(low.index + low.data.length() - high.index);
//    } else if (low.index + low.data.length() >= high.index + high.index) {
//        elem1 = low;
//    }
//}

size_t StreamReassembler::unassembled_bytes() const {
    size_t _overlap_index = 0;
    size_t _unassembled_byte = 0;
    if (_map.size() == 0) {return 0;}
    auto iter = _map.begin();
    //_overlap_index = iter->first + iter->second.length();
    //_unassembled_byte += iter->second.length();
    for (; iter != _map.end(); iter++) {
        // no overlap
        if (_overlap_index <= iter->first) {
            _overlap_index = iter->first + iter->second.length();
            _unassembled_byte += iter->second.length();
        } //overlap
        else {
            continue ;
        }
    }
    return _unassembled_byte;
}

bool StreamReassembler::empty() const { return _eof_appear_sign && _expect_index >= _eof_index; }
size_t StreamReassembler::expected_bytes_index() const{
    return _expect_index;
}

