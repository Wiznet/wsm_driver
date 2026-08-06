#!/usr/bin/env python3
from __future__ import annotations

import argparse
import random
import socket
import struct
import sys
from dataclasses import dataclass


NBNS_PORT = 137
NB_QUERY_TYPE = 0x0020
INTERNET_CLASS = 0x0001


@dataclass
class NBNSResult:
    ip_address: str
    flags: int

    @property
    def group_name(self) -> bool:
        return bool(self.flags & 0x8000)

    @property
    def node_type(self) -> str:
        node_type = (self.flags >> 13) & 0x03

        return {
            0: "B-node",
            1: "P-node",
            2: "M-node",
            3: "H-node",
        }.get(node_type, "Unknown")


def encode_netbios_name(name: str, suffix: int = 0x00) -> bytes:
    """
    Convert a NetBIOS name into the RFC 1001/1002 first-level encoding.

    Ordinary workstation/host name lookups use suffix 0x00.
    """

    normalized_name = name.upper().encode("ascii")

    if len(normalized_name) > 15:
        raise ValueError("A NetBIOS name may be at most 15 characters long.")

    # A NetBIOS name is a 15-byte name plus a 1-byte suffix.
    raw_name = normalized_name.ljust(15, b" ") + bytes([suffix])

    encoded = bytearray()

    for value in raw_name:
        encoded.append(ord("A") + ((value >> 4) & 0x0F))
        encoded.append(ord("A") + (value & 0x0F))

    # DNS form: length 0x20 + the 32 encoded bytes + the 0x00 terminator
    return b"\x20" + bytes(encoded) + b"\x00"


def create_nbns_query(
    transaction_id: int,
    name: str,
    suffix: int = 0x00,
) -> bytes:
    """
    Build an NBNS Name Query packet.
    """

    # Flags:
    #   0x0100 = Recursion Desired
    flags = 0x0100

    header = struct.pack(
        "!HHHHHH",
        transaction_id,
        flags,
        1,  # Question count
        0,  # Answer count
        0,  # Authority count
        0,  # Additional count
    )

    question = (
        encode_netbios_name(name, suffix)
        + struct.pack("!HH", NB_QUERY_TYPE, INTERNET_CLASS)
    )

    return header + question


def skip_dns_name(packet: bytes, offset: int) -> int:
    """
    Skip a DNS/NBNS name field and return the offset of the next field.
    DNS compression pointers are handled as well.
    """

    while True:
        if offset >= len(packet):
            raise ValueError("The name field of the response packet is corrupt.")

        length = packet[offset]

        # Compression pointer: the top two bits are 11
        if length & 0xC0 == 0xC0:
            if offset + 1 >= len(packet):
                raise ValueError("Invalid DNS compression pointer.")

            return offset + 2

        offset += 1

        # End of the name
        if length == 0:
            return offset

        offset += length

        if offset > len(packet):
            raise ValueError("The name length in the response packet is invalid.")


def parse_nbns_response(
    packet: bytes,
    expected_transaction_id: int,
) -> list[NBNSResult]:
    """
    Extract the registered IPv4 addresses from an NBNS Name Query response.
    """

    if len(packet) < 12:
        raise ValueError("The response packet is too short.")

    (
        transaction_id,
        flags,
        question_count,
        answer_count,
        authority_count,
        additional_count,
    ) = struct.unpack("!HHHHHH", packet[:12])

    if transaction_id != expected_transaction_id:
        raise ValueError(
            f"Transaction ID mismatch: "
            f"expected=0x{expected_transaction_id:04X}, "
            f"received=0x{transaction_id:04X}"
        )

    is_response = bool(flags & 0x8000)
    response_code = flags & 0x000F

    if not is_response:
        raise ValueError("The received packet is not an NBNS response.")

    if response_code != 0:
        response_messages = {
            1: "Format error",
            2: "Server failure",
            3: "Name error: the name is not registered.",
            4: "Unsupported request",
            5: "Request refused",
            6: "Active name error",
            7: "Name conflict error",
        }

        message = response_messages.get(
            response_code,
            f"Unknown NBNS error code {response_code}",
        )
        raise LookupError(message)

    offset = 12

    # Skip the question section
    for _ in range(question_count):
        offset = skip_dns_name(packet, offset)

        if offset + 4 > len(packet):
            raise ValueError("The question section is corrupt.")

        offset += 4  # QTYPE + QCLASS

    results: list[NBNSResult] = []

    # Parse the answer section
    for _ in range(answer_count):
        offset = skip_dns_name(packet, offset)

        if offset + 10 > len(packet):
            raise ValueError("The answer section is corrupt.")

        record_type, record_class, ttl, data_length = struct.unpack(
            "!HHIH",
            packet[offset : offset + 10],
        )
        offset += 10

        if offset + data_length > len(packet):
            raise ValueError("The answer RDATA length is invalid.")

        rdata = packet[offset : offset + data_length]
        offset += data_length

        if (
            record_type != NB_QUERY_TYPE
            or record_class != INTERNET_CLASS
        ):
            continue

        # An NB address entry is 2 bytes of NB flags + a 4-byte IPv4 address.
        # A single response may carry more than one address.
        rdata_offset = 0

        while rdata_offset + 6 <= len(rdata):
            nb_flags = struct.unpack(
                "!H",
                rdata[rdata_offset : rdata_offset + 2],
            )[0]

            ip_address = socket.inet_ntoa(
                rdata[rdata_offset + 2 : rdata_offset + 6]
            )

            results.append(
                NBNSResult(
                    ip_address=ip_address,
                    flags=nb_flags,
                )
            )

            rdata_offset += 6

    return results


def query_netbios_name(
    server: str,
    name: str,
    suffix: int = 0x00,
    timeout: float = 2.0,
    retries: int = 2,
) -> list[NBNSResult]:
    """
    Look up a name on UDP port 137 of the given NBNS server.
    """

    server_ip = socket.gethostbyname(server)
    last_error: Exception | None = None

    for attempt in range(1, retries + 1):
        transaction_id = random.randint(0, 0xFFFF)
        query_packet = create_nbns_query(
            transaction_id=transaction_id,
            name=name,
            suffix=suffix,
        )

        try:
            with socket.socket(
                socket.AF_INET,
                socket.SOCK_DGRAM,
            ) as sock:
                sock.settimeout(timeout)

                sock.sendto(
                    query_packet,
                    (server_ip, NBNS_PORT),
                )

                while True:
                    response, remote_address = sock.recvfrom(4096)

                    # Only handle responses sent by the requested NBNS server.
                    if remote_address[0] != server_ip:
                        continue

                    return parse_nbns_response(
                        response,
                        transaction_id,
                    )

        except socket.timeout:
            last_error = TimeoutError(
                f"No response from {server_ip}:{NBNS_PORT} "
                f"within {timeout:g}s. "
                f"Attempt {attempt}/{retries}"
            )

        except (OSError, ValueError, LookupError) as error:
            last_error = error
            break

    if last_error is None:
        last_error = RuntimeError("Unknown NBNS lookup error.")

    raise last_error


def parse_suffix(value: str) -> int:
    """
    Turn inputs such as 0, 00, 0x00, 20 or 0x20 into a suffix value.
    """

    normalized = value.strip().lower()

    try:
        if normalized.startswith("0x"):
            suffix = int(normalized, 16)
        else:
            # Suffixes are conventionally written in hex, so the bare form is
            # parsed as hex too.
            suffix = int(normalized, 16)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            f"Invalid suffix value: {value}"
        ) from error

    if not 0 <= suffix <= 0xFF:
        raise argparse.ArgumentTypeError(
            "The suffix must be between 0x00 and 0xFF."
        )

    return suffix


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Look up a NetBIOS Name Service name over UDP port 137."
        )
    )

    parser.add_argument(
        "-U",
        "--server",
        required=True,
        help="IP address of the NBNS server to send the query to",
    )

    parser.add_argument(
        "name",
        help="NetBIOS name to look up",
    )

    parser.add_argument(
        "--suffix",
        type=parse_suffix,
        default=0x00,
        help="NetBIOS suffix. Default: 00",
    )

    parser.add_argument(
        "--timeout",
        type=float,
        default=2.0,
        help="Seconds to wait for each response. Default: 2",
    )

    parser.add_argument(
        "--retries",
        type=int,
        default=2,
        help="Maximum number of requests. Default: 2",
    )

    args = parser.parse_args()

    if args.timeout <= 0:
        parser.error("--timeout must be greater than 0.")

    if args.retries <= 0:
        parser.error("--retries must be at least 1.")

    try:
        results = query_netbios_name(
            server=args.server,
            name=args.name,
            suffix=args.suffix,
            timeout=args.timeout,
            retries=args.retries,
        )

    except Exception as error:
        print(
            f"Lookup failed: {error}",
            file=sys.stderr,
        )
        return 1

    if not results:
        print(
            f"The response carried no IP address for the name "
            f"{args.name.upper()}.",
            file=sys.stderr,
        )
        return 2

    for result in results:
        name_type = "GROUP" if result.group_name else "UNIQUE"

        print(
            f"{result.ip_address} "
            f"{args.name.upper()}<"
            f"{args.suffix:02X}> "
            f"{name_type} "
            f"{result.node_type}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
