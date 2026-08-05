# Architecture

This document describes the current organization of the HTTP server, the
responsibilities of its components, and how data flows between them.

Decisions made during development are documented separately in
[`DevelopmentNotes.md`](DevelopmentNotes.md).

---

## Overview

The project is currently divided into five main components:

| Component | Responsibility |
|---|---|
| `HttpServer` | Own the listening socket, reconstruct complete requests, dispatch them, and transmit responses |
| `HttpParser` | Validate request framing and transform a complete raw request into an `HttpRequest` |
| `HttpRequest` | Represent the different parts of a parsed HTTP request |
| `Router` | Match a request method and path to a handler or generate a routing error response |
| `HttpResponse` | Represent and serialize an HTTP response |

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
- closing the network resources it owns;
- delegating valid parsed requests to `Router::route()`;
- transmitting serialized responses completely with `sendAll()`;

The initialization steps are separated into private methods so that each method
represents a specific networking operation.

`start()` is the public entry point for starting the server. If any step fails,
server startup is aborted.

A configured `Router` is supplied when the server is constructed. The router is
moved into the server and stored as `router_`.

The server freezes the router during construction so that its route
configuration cannot be modified while requests are being processed.

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

When the request-target contains a query string, `parse()` separates it from the
path at the first `?` character.

For example:

```text
/search?q=test
```

is represented as:

```text
path  = /search
query = q=test
```

The query remains in its raw encoded form. Percent-decoding and interpretation
of individual query parameters are not currently performed.

The parser is responsible for the HTTP structure supported by the project,
including:

- the request line;
- the HTTP method;
- the request-target;
- separating the path and query string contained in the request-target;
- the HTTP version;
- the headers;
- `Content-Length`;
- the body.


It returns a `std::optional<HttpRequest>`:

- an `HttpRequest` value when the request is accepted;
- `std::nullopt` when the request is invalid or ambiguous.

The parser does not decide whether a method is implemented or whether a route
exists. Those decisions belong to the routing layer..

### `HttpRequest`

`HttpRequest` is a data structure that represents a request after parsing.

It currently contains:

- the method;
- the path;
- the raw query string;
- the version;
- the headers;
- the body.

Header names are stored in lowercase to allow case-insensitive lookup.

The path does not include the query string. The query is stored separately
without the leading `?` character.

### `Router`

`Router` owns the route configuration and dispatches parsed requests to
handlers.

A route is identified by the combination of:

- an HTTP method;
- an exact path;
- a handler that receives an `HttpRequest` and returns an `HttpResponse`.

`addRoute()` rejects:

- unsupported registration methods;
- empty paths;
- paths that do not begin with `/`;
- empty handlers;
- duplicate method-path combinations;
- routes added after the router has been frozen.

The same path may be registered for different methods, and the same method may
be registered for different paths.

`freeze()` prevents further route registration. `HttpServer` freezes its router
during construction before request processing begins.

`route()` applies the following decision order:

1. an unknown method produces `501 Not Implemented`;
2. a method known by the server with an unknown path produces `404 Not Found`;
3. an existing path without a handler for the requested method produces
   `405 Method Not Allowed` and an `Allow` header;
4. a matching method and path execute the registered handler.

The `501` check deliberately occurs before path lookup. Therefore, an unknown
method used with an unknown path still produces `501 Not Implemented`.

The router does not receive data from sockets and does not serialize or transmit
responses. It only selects or constructs an `HttpResponse`.

### `HttpResponse`

`HttpResponse` represents an HTTP response before it is transmitted to the
client.

It currently contains:

- a status code;
- a reason phrase associated with the status code;
- a collection of response headers;
- a response body.

Headers can be added with `setHeader()`.

`serialize()` transforms the response into the HTTP/1.1 wire format expected by
the client. It generates the status line, serializes the headers, adds the
header-body separator, and appends the response body.

`Content-Length` is calculated automatically from the number of bytes in the
response body.

The serialized response is returned as a `std::string` and is then transmitted
by `HttpServer`.


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
  → determine and remember the complete request size
  → continue receiving until the request is complete
  → HttpParser::parse()
  → obtain an HttpRequest or reject the request
  → Router::route()
  → execute a matching handler or generate a routing error response
  → obtain an HttpResponse
  → add the current connection policy
  → serialize the response
  → sendAll()
  → close the client socket
```
handleClient() does not assume that one call to recv() contains one complete
HTTP request. It accumulates the received bytes and calls HttpParser::parse()
only after the complete request has been reconstructed.

Once the request head has been analyzed, the expected total request size is
stored and is not recalculated after every subsequent call to recv().

The serialized response is transmitted with sendAll(), which continues
calling send() until all response bytes have been sent or an unrecoverable
error occurs.

The connection is currently closed after processing one request. Persistent
connections are not yet supported.

For a valid parsed request, `HttpServer` delegates the application-level
decision to `Router::route()`.

The router either executes a matching handler or constructs an appropriate
`404`, `405`, or `501` response. `HttpServer` then applies the current
connection policy, serializes the response, and transmits it.

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
## Response Transmission

`HttpResponse` is responsible for constructing the HTTP representation of a
response, while `HttpServer` is responsible for transmitting its serialized
bytes.

A single call to `send()` is not guaranteed to transmit the complete response.
`HttpServer::sendAll()` therefore tracks the number of bytes already sent and
continues until the entire serialized response has been transmitted.

If `send()` is interrupted by a signal and reports `EINTR`, the operation is
retried. Any other transmission error aborts the connection.

`MSG_NOSIGNAL` is used to prevent the process from receiving `SIGPIPE` when a
client disconnects before the response has been completely transmitted.

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

### Router

The configured router is passed by value to the `HttpServer` constructor and
then moved into the `router_` member.

`HttpServer` therefore owns the router used during request processing. The
router is frozen during server construction, so its route table remains stable
throughout the server's lifetime.

---

## Parsing Behavior

### HTTP Methods

The parser accepts any method with valid syntax.

### Request-Target

Only the `origin-form` is supported.

The request-target must contain a non-empty path beginning with `/`. If a query
string is present, the parser separates it at the first `?` character.

The path is stored without the query, while the query is stored separately
without the leading `?`.

Query percent-decoding and parameter interpretation are not yet implemented.

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
- structured error responses for malformed or oversized requests;
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

The following capabilities are planned but have not yet been implemented:

- structured error-response generation for reception and parsing failures;
- a concurrency strategy for handling multiple clients;
- persistent connection management;
- controlled server shutdown.

They are not yet part of the implemented architecture.
