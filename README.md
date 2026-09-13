# TCP-Echo-Client-Server-with-IPv4-IPv6-Multicast
## Overview

This project implements a TCP Echo Client/Server using socket programming in C.

The implementation includes the features required for the assignment:

- Hostname-based server connection using `getaddrinfo()`
- IPv4 TCP support
- IPv6 TCP support
- IPv4 multicast
- IPv6 multicast
- Timestamped message exchange
- TCP Round-Trip Time (RTT) measurement
- Multiple clients can receive multicast messages

The program uses standard socket APIs such as `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `sendto()`, `recvfrom()`, and `select()`.

---

## Files

- `Server.c` — Echo server source code
- `Client.c` — Echo client source code

---

## Requirements

Linux/WSL environment with:

- GCC
- Standard POSIX socket libraries
- IPv4/IPv6 networking support

No additional third-party libraries are required.

---

## Compilation

Compile the server:

```bash
gcc -Wall -Wextra Server.c -o server
```

Compile the client:

```bash
gcc -Wall -Wextra Client.c -o client
```

---

#  Hostname Support Using getaddrinfo()

The client accepts the server hostname as a command-line argument:

```bash
./client <server-hostname> <server-port>
```

Example:

```bash
./client 127.0.0.1.nip.io 12345
```

The client uses `getaddrinfo()` with `AF_UNSPEC`, allowing the returned address to be IPv4 or IPv6. It then tries the addresses returned by `getaddrinfo()` until a connection succeeds.

Relevant implementation:

```c
getaddrinfo(serverName, serverPort, &hints, &result);
```

### Demonstration

1. Start the server:

```bash
./server 12345
```

2. Run the client using a hostname instead of a numeric IP address:

```bash
./client 127.0.0.1.nip.io 12345
```

3. The client should display a successful connection, for example:

```text
Connected to 127.0.0.1.nip.io:12345
```

4. For the Wireshark demonstration, capture the DNS traffic while starting the hostname-based client.

A useful Wireshark display filter is:

```text
dns.qry.name == "127.0.0.1.nip.io"
```

The capture should show DNS queries for A/AAAA records and the corresponding DNS responses. The A response resolves:

```text
127.0.0.1.nip.io -> 127.0.0.1
```

This demonstrates hostname resolution before the TCP connection.
