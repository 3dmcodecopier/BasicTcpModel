#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _ms_since_last_segment_received; }

bool TCPConnection::send_segments() {
    bool isSent = false;
//    if (_sender.segments_out().empty()){
//        _sender.send_empty_segment();
//    }
//    if (_rst) {
//        fill_queue(_sender.segments_out());
//        return isSent;
//    }
    while (!_sender.segments_out().empty()) {
        isSent = true;
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_win(segment);
        _segments_out.push(segment);
    }
    return isSent;
}

void TCPConnection::set_ack_win(TCPSegment &seg) {
    if (_receiver.ackno().has_value()) {
        seg.header().ackno = _receiver.ackno().value();
        seg.header().ack = true;
    }
    seg.header().win = static_cast<uint16_t>(_receiver.window_size());
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _ms_since_last_segment_received = 0;
    //if (!_active) {return ;}
    //check rst bit
    if (seg.header().rst) {
//        _sender.stream_in().set_error();
//        _receiver.stream_out().set_error();
//        _active = false;
        set_rst_segment();
        return ;
    }
    //give seg to receiver
    _receiver.segment_received(seg);

//    if (check_inbound_end() && !_sender.stream_in().eof()) {
//        _linger_after_streams_finish = false;
//    }

    //check ack, very important, improve TCP efficiency
    //receive segment(with ack), update _sender and send segments
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        send_segments();
    }
    //send ack seg
    if (seg.length_in_sequence_space() > 0) {
        // handle the SYN/ACK case
        _sender.fill_window();
        bool isSend = send_segments();
        // send at least one ack message
        if (!isSend) {
            _sender.send_empty_segment();
            TCPSegment ACKSeg = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_win(ACKSeg);
            _segments_out.push(ACKSeg);
        }
    }
    test_end();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (!data.size()) { return 0; }
    size_t connection_write = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return connection_write;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _ms_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.segments_out().size() != 0) {
        TCPSegment retransSeg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_win(retransSeg);
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            retransSeg.header().rst = true;
            //_segments_out.push(set_rst_segment());
            set_rst_segment();
//            _active = false;
//            _sender.stream_in().set_error();
//            _receiver.stream_out().set_error();
        }
        _segments_out.push(retransSeg);
    }
    //TCPConnection end(difficult)
    test_end();

//    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof() && _sender.next_seqno_absolute() > 0) {
//        _linger_after_streams_finish = false;
//    } else if (_receiver.stream_out().eof() && _sender.stream_in().eof() && unassembled_bytes() == 0 &&
//               bytes_in_flight() == 0) {
//        if (!_linger_after_streams_finish)
//            _active = false;
//        else if (_ms_since_last_segment_received >= 10 * _cfg.rt_timeout)
//            _active = false;
//    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    //if (_sender.next_seqno_absolute() != 0) {return ;}
    _sender.fill_window();
    _active = true;
    send_segments();
}

TCPSegment TCPConnection::set_rst_segment() {
    _active = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    //generate the seqno for the rstSegment
    _sender.send_empty_segment();
    TCPSegment rstSegment = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ack_win(rstSegment);
    rstSegment.header().rst = true;
    return rstSegment;
}

bool TCPConnection::check_inbound_end() {
    return _receiver.stream_out().input_ended() && _receiver.unassembled_bytes() == 0;
}

bool TCPConnection::check_outbound_end() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _segments_out.push(set_rst_segment());
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::test_end() {
    //state CLOSE_WAIT
    if (check_inbound_end() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if (check_inbound_end() && check_outbound_end()) {
        if (!_linger_after_streams_finish) {
            //state CLOSED
            _active = false;
        }
        else if (_ms_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}