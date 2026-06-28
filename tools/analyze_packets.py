#!/usr/bin/env python3
"""
MeshCore Packet Capture Analyzer
For security research on your own mesh traffic.

Binary format per packet:
  timestamp (4 bytes, uint32_t)
  snr (1 byte, int8_t - multiply by 0.25 to get dB)
  rssi (2 bytes, int16_t)
  length (1 byte, uint8_t)
  packet_data (length bytes)

Usage:
  ./analyze_packets.py /var/lib/meshcore/packet_capture.bin
"""

import struct
import sys
from datetime import datetime

def parse_packet_header(data):
    """Parse packet header byte"""
    route = data[0] & 0x03
    ptype = (data[0] >> 2) & 0x0F
    pver = (data[0] >> 6) & 0x03

    route_names = {0: 'TRANSPORT_FLOOD', 1: 'FLOOD', 2: 'DIRECT', 3: 'TRANSPORT_DIRECT'}
    type_names = {
        0x00: 'REQ', 0x01: 'RESPONSE', 0x02: 'TXT_MSG', 0x03: 'ACK',
        0x04: 'ADVERT', 0x05: 'GRP_TXT', 0x06: 'GRP_DATA', 0x07: 'ANON_REQ',
        0x08: 'PATH', 0x09: 'TRACE', 0x0A: 'MULTIPART', 0x0B: 'CONTROL',
        0x0F: 'RAW_CUSTOM'
    }

    return {
        'route': route_names.get(route, f'UNKNOWN({route})'),
        'type': type_names.get(ptype, f'UNKNOWN({ptype})'),
        'version': pver
    }

def analyze_capture(filename):
    """Analyze captured packets"""
    with open(filename, 'rb') as f:
        packet_count = 0
        channel_stats = {}

        while True:
            # Read header
            header = f.read(8)
            if len(header) < 8:
                break

            timestamp, snr, rssi, length = struct.unpack('<IBhB', header)

            # Read packet data
            packet_data = f.read(length)
            if len(packet_data) < length:
                break

            packet_count += 1
            snr_db = snr / 4.0
            dt = datetime.fromtimestamp(timestamp)

            # Parse packet
            pkt_info = parse_packet_header(packet_data)

            print(f"\n[{packet_count}] {dt.strftime('%Y-%m-%d %H:%M:%S')}")
            print(f"  Type: {pkt_info['type']} ({pkt_info['route']})")
            print(f"  Signal: SNR={snr_db:.1f}dB RSSI={rssi}dBm")
            print(f"  Size: {length} bytes")

            # Extract channel hash for group messages
            if pkt_info['type'] in ['GRP_TXT', 'GRP_DATA'] and len(packet_data) > 3:
                # Packet format: header(1) + payload_len(2) + path_len(2) + transport_codes(4) + path + payload
                # payload[0] is the channel hash
                payload_start = 1 + 2 + 2 + 4  # Skip header, payload_len, path_len, transport_codes
                if len(packet_data) > payload_start:
                    channel_hash = packet_data[payload_start]
                    print(f"  Channel: 0x{channel_hash:02X}")
                    channel_stats[channel_hash] = channel_stats.get(channel_hash, 0) + 1

            # Show raw packet hex (first 64 bytes)
            hex_str = ' '.join(f'{b:02X}' for b in packet_data[:64])
            print(f"  Data: {hex_str}{'...' if length > 64 else ''}")

        print(f"\n\n=== Summary ===")
        print(f"Total packets: {packet_count}")

        if channel_stats:
            print(f"\nChannel activity:")
            for ch, count in sorted(channel_stats.items(), key=lambda x: -x[1]):
                print(f"  0x{ch:02X}: {count} messages")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)

    analyze_capture(sys.argv[1])
