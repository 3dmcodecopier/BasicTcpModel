#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPTimer {
  private:
    //! true : timer start , false : timer not start
    bool _start;
    unsigned int init_time;
    //! Transmission time
    unsigned int transmission_time;
    //! retransmission timeout
    unsigned int RTO;
  public:
    //! Number of consecutive retransmissions
    unsigned int num_of_retransmission;
    TCPTimer(unsigned int time)
        : _start(false)
        , init_time(time)
        , transmission_time(0)
        , RTO(init_time)
        , num_of_retransmission(0) {}
    bool running() { return _start; }
    void close() {
        _start = false;
        num_of_retransmission = 0;
    }
    void start() {
        _start = true;
        RTO = init_time;
        transmission_time = 0;
        num_of_retransmission = 0;
    }
    //! if window == 0 then keep RTO , otherwise double RTO
    void doubleOrkeep_RTO_and_restart(const size_t window) {
        if (!running())
            return;
        if (window != 0)
            RTO *= 2;
        transmission_time = 0;
        num_of_retransmission++;
    }
    bool timeout(const size_t ms_since_last_tick) {
        if (!running())
            return false;
        if (ms_since_last_tick + transmission_time >= RTO)
            return true;
        transmission_time += ms_since_last_tick;
        return false;
    }
};
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
    uint64_t _ackno;
    size_t _window_size;
    uint64_t _bytes_in_flight;
    TCPTimer timer;
    std::queue<TCPSegment> _segments_track{};
    void send_no_empty_segments(TCPSegment &seg);


  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
