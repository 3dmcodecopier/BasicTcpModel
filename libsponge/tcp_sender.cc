#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <random>
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
using namespace std;
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout
                     ,const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _ackno(0)
    , _window_size(1)
    , _bytes_in_flight(0)
    , timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::send_no_empty_segments(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_track.push(seg);
    if (!timer.running()) {
        timer.start();
    }
}

void TCPSender::fill_window() {
    /*Status: CLOSED -> stream waiting to begin*/
    if (_next_seqno == 0) {
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = next_seqno();
        send_no_empty_segments(seg);
    }
    /*Status: SYN_SENT -> stream start but nothing acknowledged*/
    else if (_next_seqno == _bytes_in_flight) {
        return;
    }
    size_t window_size = _window_size == 0 ? 1 : _window_size;
    size_t remain = 0;
    //! 这里window_size 一定是大于 (_next_seqno - _ackno),不用担心溢出问题。文章后面解释
    while ((remain = window_size - (_next_seqno - _ackno))) {
        TCPSegment seg;
        size_t len = TCPConfig::MAX_PAYLOAD_SIZE > remain ? remain : TCPConfig::MAX_PAYLOAD_SIZE;
        /*Status: SYN_ACKED -> stream ongoing*/
        if (!_stream.eof()) {
            seg.payload() = Buffer(_stream.read(len));
            if (_stream.eof() && remain - seg.length_in_sequence_space() > 0)
                seg.header().fin = true;
            if (seg.length_in_sequence_space() == 0)
                return;
            send_no_empty_segments(seg);
        }
        /*Status: SYN_ACKED -> stream ongoing (stream has reached EOF but FIN hasn't been sent yet)*/
        else if (_stream.eof()) {
            if (_next_seqno < _stream.bytes_written() + 2) {
                seg.header().fin = true;
                send_no_empty_segments(seg);
            }
            /*Status: FIN_SENT and FIN_ACKED both do nothing Just return */
            else
                return;
        }
    }
}

void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _ackno);
    //! 超出范围 _next_seqno还没发呢，哪来的abs_ackno > _next_seqno
    if (abs_ackno > _next_seqno) {
        return;
    }
    _window_size = static_cast<size_t>(window_size);
    //! 比abs_ackno大的先来了
    if (abs_ackno <= _ackno) {
        return;
    }
    _ackno = abs_ackno;
    //! 成功接受到新的ackno
    timer.start();
    while (!_segments_track.empty()) {
        TCPSegment seg = _segments_track.front();
        if (ackno.raw_value() < seg.header().seqno.raw_value() + static_cast<uint32_t>(seg.length_in_sequence_space()))
            break;
        _bytes_in_flight -= seg.length_in_sequence_space();
        _segments_track.pop();
    }
    fill_window();
    if (_segments_track.empty()) {
        timer.close();
    }
}

void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!timer.running() || !timer.timeout(ms_since_last_tick))
        return;
    if (_segments_track.empty()) {
        timer.close();
        return;
    }
    timer.doubleOrkeep_RTO_and_restart(_window_size);
    _segments_out.push(_segments_track.front());
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return timer.num_of_retransmission;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
