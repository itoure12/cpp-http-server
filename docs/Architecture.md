# Architecture

This document describes the current organization of the HTTP server, the
responsibilities of its components, and how data flows between them.

Decisions made during development are documented separately in
[`DevelopmentNotes.md`](DevelopmentNotes.md).

---

## Overview

The project is currently divided into three main components:

| Component | Responsibility |
|---|---|
| `HttpServer` | Create the listening socket, accept clients, and receive network data |
| `HttpParser` | Transform a complete raw HTTP request into an `HttpRequest` |
| `HttpRequest` | Represent the different parts of a validated HTTP request |

This separation prevents networking logic and HTTP parsing logic from being
mixed within a single class.

---

## Current Components

### `HttpServer`

`HttpServer` owns the server's networking infrastructure.

Its current responsibilities are:

- creating the TCP listening socket;
- configuring socket options;
- binding the socket to a port;
- placing the socket in listening mode;
- accepting incoming connections;
- passing each client socket to `handleClient()`;
- receiving data sent by the client;
- closing the network resources it owns.

The initialization steps are separated into private methods so that each method
represents a specific networking operation.

`start()` is the public entry point for starting the server. If any step fails,
server startup is aborted.

### `HttpParser`

`HttpParser` transforms a string containing a complete HTTP request into an
`HttpRequest` structure.

Its interfaces are `static` methods because parsing does not depend on any state
preserved between requests.

`determineRequestSize()` examines a complete request head, validates the framing
information, and calculates the total number of bytes expected from
`Content-Length`.

`parse()` is called only after that number of bytes has been received. It
transforms the complete raw request into an `HttpRequest`.

The parser is responsible for the HTTP structure supported by the project,
including:

- the request line;
- the HTTP method;
- the request-target;
- the HTTP version;
- the headers;
- `Content-Length`;
- the body.

It returns a `std::optional<HttpRequest>`:

- an `HttpRequest` value when the request is accepted;
- `std::nullopt` when the request is invalid or ambiguous.

The parser does not decide whether a method is implemented or whether a route
exists. Those decisions will belong to the routing layer.

### `HttpRequest`

`HttpRequest` is a data structure that represents a request after parsing.

It currently contains:

- the method;
- the path or request-target;
- the version;
- the headers;
- the body.

Header names are stored in lowercase to allow case-insensitive lookup.

---

## Current Connection Flow

The currently implemented flow is:

```text
main()
  → construct HttpServer
  → HttpServer::start()
  → create and configure the listening socket
  → bind()
  → listen()
  → acceptLoop()
  → accept()
  → handleClient()
  → receive and accumulate TCP fragments
  → detect the end of the request head
  → determine the complete request size
  → continue receiving until the request is complete
  → HttpParser::parse()
  → obtain an HttpRequest or reject the request
  → close the client socket
```
handleClient() does not assume that one call to recv() contains one complete
HTTP request. It accumulates the received bytes and calls HttpParser::parse()
only after the complete request has been reconstructed.

The connection is currently closed after processing one request. Persistent
connections are not yet supported.

---

## Separation Between TCP and HTTP

TCP provides a byte stream. It does not preserve HTTP message boundaries.

A request sent in a single operation by a client may therefore be received by
the server across multiple calls to `recv()`. Conversely, a call to `recv()`
does not necessarily represent a complete request.

The implemented separation is:

### `HttpServer::handleClient()`

Responsible for network reception:

- calling `recv()` as many times as necessary;
- accumulating only the bytes actually received;
- detecting the end of the headers;
- determining the expected total size;
- enforcing size limits;
- detecting errors and disconnections;
- calling the parser only once the request is complete.

### `HttpParser::parse()`

Responsible for HTTP interpretation:

- analyzing an already complete request;
- validating the supported HTTP/1.1 subset;
- producing an `HttpRequest`;
- rejecting invalid or ambiguous representations.

This boundary allows parsing to be tested independently of sockets.

---

## Resource Ownership

### Listening Socket

The listening socket is owned by `HttpServer`.

`serverSocket_` is initialized to `-1`, which represents the absence of a valid
file descriptor. When a valid socket exists, the `HttpServer` destructor is
responsible for closing it.

The lifetime of the system resource is therefore tied to the lifetime of the
`HttpServer` object.

### Client Sockets

Each client socket is returned by `accept()` and then passed to
`handleClient()`.

After the connection has been handled, the socket must be closed exactly once,
including when a receive or parsing error occurs.

---

## Parsing Behavior

### HTTP Methods

The parser accepts any method with valid syntax.

### Request-Target

Only the `origin-form`, whose target begins with `/`, is supported.

### Headers

Header names are normalized to lowercase.

Any duplicate header is rejected.

### `Content-Length`

`Content-Length` is converted to `std::size_t`.

The value must be non-empty, non-negative, fully convertible, and representable
by `std::size_t`.

---

## Current Scope

### Features Not Yet Implemented

- concurrent connection handling;
- a thread pool;
- routing;
- HTTP response generation and serialization;
- complete response transmission;
- persistent connections;
- controlled server shutdown.

This list tracks the project's progress and will be updated as these features
are implemented.

### Deliberate Scope Decisions

The HTTP subset currently supported by the server is based on several deliberate
scope decisions.

Their rationale and possible future evolution are documented in the
“Accepted Simplifications” sections of
[`DevelopmentNotes.md`](DevelopmentNotes.md).

---

## Planned Architecture

The following components are planned but have not yet been implemented:

- `Router`, to map a method and route to a handler;
- `HttpResponse`, to represent an HTTP response;
- a response serialization mechanism;
- a send function that guarantees transmission of all bytes;
- a concurrency strategy for handling multiple clients;
- controlled server shutdown.

They are not yet part of the implemented architecture.
