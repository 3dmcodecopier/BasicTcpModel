#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    //if there is no mac_addr in _arp_map
    if (_arp_map.find(next_hop_ip) == _arp_map.end()) {
        //send ARP Message
        if (_waiting_arp_response_IP_map.find(next_hop_ip) == _waiting_arp_response_IP_map.end()) {
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;

            EthernetFrame frame;
            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.payload() = arp_request.serialize();
            _frames_out.push(frame);
            _waiting_arp_response_IP_map[next_hop_ip] = 5 * 1000;
        }
        Datagram_nexthop new_datagram = {next_hop, dgram};
        _waiting_arp_IPV4Datagram.push_back(new_datagram);
    }
    else {
        EthernetFrame frame;
        frame.header().src = _ethernet_address;
        frame.header().dst = _arp_map[next_hop_ip].mac_addr;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {return nullopt;}
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMsg;
        if (arpMsg.parse(frame.payload()) != ParseResult::NoError) {return nullopt;}

        _arp_map[arpMsg.sender_ip_address] = {arpMsg.sender_ethernet_address, 30 * 1000};
        for (auto iter = _waiting_arp_IPV4Datagram.begin(); iter != _waiting_arp_IPV4Datagram.end();) {
            if (iter->next_hop.ipv4_numeric() == arpMsg.sender_ip_address) {
                send_datagram(iter->datagram, iter->next_hop);
                iter = _waiting_arp_IPV4Datagram.erase(iter);
            }
            else iter++;
        }
        _waiting_arp_response_IP_map.erase(arpMsg.sender_ip_address);
        // if arp request
        if (arpMsg.opcode == ARPMessage::OPCODE_REQUEST && arpMsg.target_ip_address == _ip_address.ipv4_numeric()) {
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.target_ip_address = arpMsg.sender_ip_address;
            arp_reply.target_ethernet_address = arpMsg.sender_ethernet_address;

            EthernetFrame reply_frame;
            reply_frame.header().src = _ethernet_address;
            reply_frame.header().dst = arpMsg.sender_ethernet_address;
            reply_frame.header().type = EthernetHeader::TYPE_ARP;
            reply_frame.payload() = arp_reply.serialize();
            _frames_out.push(reply_frame);
        }
    } else if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram internetDatagram;
        if (internetDatagram.parse(frame.payload()) != ParseResult::NoError) {return nullopt;}
        return internetDatagram;
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    //delete expired items in _arp_map
    for (auto iter = _arp_map.begin(); iter != _arp_map.end();) {
        if (iter->second.ttl <= ms_since_last_tick) {
            iter = _arp_map.erase(iter);
        } else {
            iter->second.ttl -= ms_since_last_tick;
            iter++;
        }
    }
    //delete expired items in std::vector<Datagram_nexthop> _waiting_arp_IPV4Datagram;
    for (auto iter = _waiting_arp_response_IP_map.begin(); iter != _waiting_arp_response_IP_map.end();) {
        if (iter->second <= ms_since_last_tick) {
            //resend ARP request
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = iter->first;

            EthernetFrame frame;
            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.payload() = arp_request.serialize();
            _frames_out.push(frame);
            iter->second = 5 * 1000;
        }
        else {
            iter->second -= ms_since_last_tick;
            iter++;
        }
    }
}
