# Development Notes

This document records the main stages of the server's development, the
technical decisions made, the problems encountered, and the methods used to
validate each phase.

It describes the project's evolution. The currently implemented architecture
is documented separately in `Architecture.md`.



---

## Phase 1 — TCP Server Foundations

> Status: completed

### Objective

Build a sequential TCP server capable of creating a listening socket, binding
it to a port, accepting client connections, and correctly managing the
lifetime of the server socket.

### Responsibilities and Limitations

The `HttpServer` class is responsible for the main networking infrastructure:

- creating the listening socket;
- configuring its options;
- binding it to the requested port;
- placing it in listening mode;
- accepting client connections;
- closing the server socket when it is no longer in use.

At this stage, the server does not yet process HTTP requests. It accepts a
connection, passes it to `handleClient()`, and then closes the client socket.

### Technical Decisions

- Encapsulated the networking infrastructure in an `HttpServer` class to group
  the server's state with the operations that act on that state.

- Stored the port in a `std::uint16_t` member because a TCP port number is an
  unsigned 16-bit value.

- Initialized `serverSocket_` to `-1` to explicitly represent the absence of a
  valid socket.

- Closed the listening socket in the `HttpServer` destructor. The lifetime of
  the system resource is therefore tied to the lifetime of the object that owns
  it.

- Split initialization into several private methods:

  - `createSocket()`;
  - `configureSocket()`;
  - `bindSocket()`;
  - `listenForConnections()`;
  - `acceptLoop()`.

  Each method represents a specific step and can report failure to `start()`.

- Used a `bool` return value for the initialization steps. A `false` value
  immediately aborts server startup.

- Enabled `SO_REUSEADDR` so the server can be restarted more easily after
  shutdown without unnecessarily waiting for the local address to be fully
  released.

- Used a loop around `accept()` so the server can accept multiple successive
  connections.

- Delegated connection handling to `handleClient()` to separate client
  acceptance from client processing.

### Accepted Simplifications

- The server still operates sequentially: one connection is handled before the
  next one is accepted.

- No thread pool or I/O multiplexing mechanism is used yet.

- The accept loop does not yet have a controlled shutdown mechanism.

- System errors are currently displayed with `perror()` instead of being
  propagated through a structured error-handling system.

- The `listen()` backlog is set to a simple constant.

### Problems Encountered and Fixes

- The server socket needed a recognizable initial value before the call to
  `socket()`. The value `-1` is used as a sentinel to avoid closing an invalid
  file descriptor.

- The different server creation steps initially formed a single sequence of
  system operations. They were separated into private methods to make their
  responsibilities and failure modes easier to understand.

- Closing the socket manually in several places would have made its lifetime
  management fragile. The listening socket is therefore released by the
  `HttpServer` destructor.

### Validation Evidence

- The server builds successfully with CMake.
- The server starts on port `8080`.
- A local connection to the server has been validated.
- `acceptLoop()` can accept multiple successive connections.

### What I Learned

- How to encapsulate a system resource in a C++ class.
- How to use a constructor to establish an object's initial state.
- How to use a destructor to release a resource owned by an object.
- How to separate a class's public interface from its internal operations.
- How to decompose a networking procedure into distinct responsibilities.
- How to explicitly represent an invalid file descriptor with `-1`.

---

## Phase 2 — HTTP Request Reception and Parsing

> Status: completed
> Part 1 completed: `HttpParser`
> Part 2 completed: TCP reception in `HttpServer::handleClient()`

### Part 1 — HttpParser

#### Objective

Transform an already complete raw HTTP request into an `HttpRequest` structure
while validating the HTTP/1.1 subset supported by the server and rejecting the
invalid or ambiguous forms covered by the parser.

#### Responsibilities and Limitations

`HttpParser` validates the request syntax supported by the server.

It does not decide whether an HTTP method is implemented or whether a route
exists. Those semantic decisions will later belong to the `Router`.

The parser receives an already complete request. Receiving data from TCP,
assembling its fragments, and determining the end of the message belong to
`HttpServer::handleClient()`.

#### Technical Decisions

- Represented the result with `std::optional<HttpRequest>`. A valid request
  produces an `HttpRequest`, while a rejected request produces `std::nullopt`.

- Declared `HttpParser::parse()` as a `static` method because its result depends
  only on the received request and not on state belonging to an `HttpParser`
  instance.

- Separated the HTTP data into an `HttpRequest` structure containing the method,
  target, version, headers, and body.

- Used `std::from_chars` instead of `std::stoul` to parse `Content-Length`: it
  throws no exceptions, performs no allocation, and makes it possible to verify
  that the entire value was consumed.

- Used `std::optional<std::size_t>` for `parseContentLength()`. The function
  receives an existing value and returns either a valid length, including zero,
  or `std::nullopt` if the value is invalid.

- Accepted unknown HTTP methods. The parser validates the method's syntax,
  while the future `Router` will decide whether it is supported.

- Normalized header names to lowercase to allow case-insensitive lookup.

- Rejected all duplicate headers, including duplicates with different casing,
  to avoid an ambiguous representation in `HttpRequest`.

- Rejected spaces and tabs in header names, including the forbidden whitespace
  before the `:` character.

- Used `endOfHeadersFound` to distinguish an empty line that actually terminates
  the headers from a premature end of the data.

- Verified that the amount of body data received matches the value declared by
  `Content-Length`.

#### Accepted Simplifications

- Only `HTTP/1.1` is accepted. A more complete server could distinguish an
  invalid request from an unsupported HTTP version and produce the appropriate
  response.

- Only the `origin-form` of the request-target is supported: the target must
  begin with `/`. The `absolute-form`, `authority-form`, and `asterisk-form` are
  not yet supported.

- All duplicate headers are rejected. A more complete implementation could
  combine certain repeatable fields when allowed by their specification.

- The current header-name validation rejects spaces and tabs but does not yet
  validate the complete set of characters defined by the HTTP `token` grammar.

#### Problems Encountered and Fixes

- The initial contract for `Content-Length` did not clearly distinguish between
  an absent header and an invalid value. The header lookup is now performed in
  `parse()`, while `parseContentLength()` validates only a value that is already
  present.

- Returning zero on failure would have confused an error with the valid
  `Content-Length: 0` value. Using `std::optional<std::size_t>` removes this
  ambiguity.

- Optional whitespace was removed from the beginning of the value but not from
  the end. Trimming now handles both ends.

- The read loop could reach the end of the data without encountering the empty
  line that separates the headers from the body. The `endOfHeadersFound` flag
  now makes it possible to reject such an incomplete request.

- Insertion with `unordered_map::operator[]` could have silently replaced an
  existing header. The insertion operation now used makes it possible to detect
  and reject duplicates.

#### Validation Evidence

- All 27 GoogleTest tests pass.
- The CMake build succeeds.
- Valid and invalid cases are covered by the parser tests.

Additional validation planned:

- compile with `-Wall`, `-Wextra`, and `-Wpedantic`;
- apply the same warning options to the tests;
- add tests related to network reception.

#### What I Learned

- The separation between syntax validation and application-level decisions.
- The usefulness of a `static` method when no object state is required.
- The difference between `std::from_chars` and exception-based conversions.
- How to use `std::optional` to explicitly represent failure.
- How to use `std::string_view` to inspect a string without copying it.
- How `data()` and the `begin` and `end` pointers passed to
  `std::from_chars` work.
- The importance of framing ambiguities when processing HTTP messages.
- The difference between a loop ending because of an empty line and one ending
  because the stream has been exhausted.

### Part 2 — TCP Reception in `handleClient()`

#### Implemented Behavior

`HttpServer::handleClient()` now receives and accumulates TCP data until one
complete HTTP request has been obtained.

The reception loop:

- calls `recv()` repeatedly;
- appends only the number of bytes actually received;
- detects the end of the request head with `\r\n\r\n`;
- uses `HttpParser::determineRequestSize()` to calculate the complete request
  size from `Content-Length`;
- continues receiving while the request is incomplete;
- rejects data exceeding the expected request size;
- calls `HttpParser::parse()` only after the complete request has been received;
- closes the client socket on both success and failure.
- stores the expected request size after the request head has been analyzed;
- avoids analyzing the same request head again during subsequent calls to
  `recv()`;
- retries `recv()` when it is interrupted with `errno == EINTR`;

The loop establishes the following post-condition: when execution continues
after the loop, `rawRequest` contains exactly one complete HTTP request.

#### Size Protections

The reception logic enforces separate limits for:

- the HTTP request head through `MAX_REQUEST_HEAD_SIZE`;
- the request body through `MAX_BODY_SIZE`.

A request is rejected if:

- its head exceeds the configured limit;
- its `Content-Length` is invalid;
- it contains duplicate `Content-Length` headers;
- its announced body exceeds the configured limit;
- additional bytes are received after the expected end of the request.

#### Error Handling

The client connection is closed when:

- `recv()` reports an error;
- the client disconnects before sending a complete request;
- the request head is invalid;
- the request exceeds a configured size limit;
- `HttpParser::parse()` rejects the completed request.

Valid requests now receive an HTTP response through the functionality added in
Phase 3. Invalid requests are still handled by closing the connection without
sending a structured HTTP error response.

#### Manual Validation

The TCP reception path was validated manually with a running server.

The following scenarios succeeded:

- `GET /` was received and parsed correctly;
- `POST /test` with `Content-Length: 5` and body `hello` was received correctly;
- the invalid request `GARBAGE\r\n\r\n` was rejected;
- the open file descriptor count remained at `4` before and after 20 requests;
- a fragmented body sent as `he`, followed one second later by `llo`, was
  accumulated and parsed correctly as `POST /split`.

At the end of Phase 2, `curl` reported `Empty reply from server` because no
response-generation mechanism existed yet. Phase 3 subsequently added response
serialization and transmission.

#### Automated Testing Decision

Socket-level automated tests using `socketpair()` were considered during this
phase but have not yet been implemented.

Phase 3 added unit tests for response generation and manual end-to-end tests
covering real TCP reception and response transmission. Automated socket-level
integration tests remain planned.

#### Known Limitations

The following limitations are documented for later phases:

- `recv()` has no timeout, so an incomplete client can block the sequential
  server indefinitely;
- persistent `accept()` failures could produce a busy loop;
- `acceptLoop()` has no shutdown condition, so the server cannot yet terminate
  gracefully and return normally from `start()`.

 ---

## Phase 3 — HTTP Response Generation and Transmission

> Status: completed

### Objective

Complete the server's first request-response cycle by representing HTTP
responses, serializing them into HTTP/1.1 wire format, and guaranteeing that
all serialized bytes are transmitted to the client.

### Responsibilities and Limitations

`HttpResponse` represents a response before it is transmitted.

It is responsible for:

- storing the response status;
- storing the response headers;
- storing the response body;
- calculating `Content-Length`;
- serializing the response into HTTP/1.1 format.

`HttpServer` remains responsible for network transmission.

After receiving and parsing one complete request, `handleClient()` constructs an
`HttpResponse`, serializes it, passes the serialized data to `sendAll()`, and
closes the connection.

### Technical Decisions

- Introduced an `HttpResponse` class so that HTTP response construction is
  separated from socket transmission.

- Provided `setHeader()` so response headers can be added before serialization.

- Made `serialize()` generate:

  - the HTTP status line;
  - the response headers;
  - the empty line separating the headers from the body;
  - the response body.

- Calculated `Content-Length` automatically from the number of bytes in the
  response body. This prevents the declared length from becoming inconsistent
  with the actual body.

- Returned the serialized response as a `std::string`, which owns the complete
  byte sequence until transmission finishes.

- Introduced `HttpServer::sendAll()` because a single call to `send()` is not
  guaranteed to transmit the complete response.

- Used a `std::size_t` counter to track the number of bytes already transmitted.

- Passed the response data to `sendAll()` as a `std::string_view` so the function
  can inspect the serialized data without making another copy.

- Retried `send()` when it fails with `errno == EINTR`, because an interrupted
  system call does not necessarily represent a connection failure.

- Used `MSG_NOSIGNAL` to prevent the server process from receiving `SIGPIPE` if
  the client disconnects during response transmission.

- Added equivalent `EINTR` handling to the reception loop around `recv()`.

- Explicitly added the `Connection: close` header because the server currently
  processes only one request per connection.

### Problems Encountered and Fixes

- A single successful call to `send()` could have transmitted fewer bytes than
  requested. `sendAll()` now repeats the operation until the complete response
  has been transmitted.

- Repeating `send()` requires advancing the data pointer and reducing the number
  of remaining bytes. The transmitted-byte counter is used for both operations.

- A client disconnecting during transmission could have caused `SIGPIPE` to
  terminate the process. `MSG_NOSIGNAL` prevents that signal for the call to
  `send()`.

- A system call interrupted by a signal was initially treated as a permanent
  failure. Both `recv()` and `send()` now retry when the reported error is
  `EINTR`.

### Accepted Simplifications

- Every valid request currently receives the same `200 OK` response.

- The response body is currently fixed and does not yet depend on the requested
  method or path.

- Routing and semantic validation are deferred to Phase 4.

- Invalid requests do not yet receive structured responses such as `400 Bad
  Request` or `413 Content Too Large`.

- The connection is closed after one response. Persistent HTTP connections are
  not yet supported.

- Client sockets are still closed manually along the different execution paths.
  A dedicated RAII wrapper may be introduced later.

### Validation Evidence

- The complete project builds successfully with CMake.

- All 32 tests from two GoogleTest suites pass.

- A real `GET /` request sent with `curl` produced a valid response containing:

  - `HTTP/1.1 200 OK`;
  - `Connection: close`;
  - `Content-Type: text/plain`;
  - the automatically calculated `Content-Length`;
  - the expected response body.

-A `POST /test` request containing the five-byte body `hello` was received,
parsed, and answered successfully.

- A fragmented `POST /split` request was sent with the request head, `bon`, and
  `jour` transmitted separately.

- The server waited until all seven body bytes had been received before parsing
  the request and transmitting the response.

This final fragmented-request test validates the complete path:

```text
multiple recv() calls
→ request accumulation
→ remembered expected size
→ complete request parsing
→ response construction
→ serialization
→ complete transmission 
```
#### What I Learned
- Why one call to send() cannot be assumed to transmit an entire buffer.
- How to implement complete transmission using a byte counter.
- How pointer arithmetic identifies the first byte that has not yet been sent.
- Why EINTR represents a retryable interruption rather than a permanent network failure.
- How MSG_NOSIGNAL protects a server from SIGPIPE.
- How to separate HTTP response representation from network transmission.
- Why Content-Length should be derived automatically from the response body.
- How to validate TCP fragmentation with a real client sending one request across multiple writes.

---

## Phase 4 — Routing and Request Dispatch

> Status: completed

### Objective

Introduce a routing layer capable of mapping a parsed HTTP request to an
application handler and producing an appropriate HTTP response when no matching
route exists.

This phase replaces the fixed response previously constructed directly by
`HttpServer::handleClient()` with a response selected by `Router`.

### Responsibilities and Limitations

`Router` owns the route configuration and performs application-level request
dispatch.

A route associates:

- an HTTP method;
- an exact path;
- a handler receiving an `HttpRequest` and returning an `HttpResponse`.

`Router` is responsible for:

- registering routes before request processing begins;
- rejecting invalid or duplicate route registrations;
- matching a request by method and path;
- executing the corresponding handler;
- producing `404 Not Found`, `405 Method Not Allowed`, or
  `501 Not Implemented` responses when appropriate.

`Router` does not:

- receive bytes from sockets;
- parse raw HTTP requests;
- serialize responses;
- transmit response bytes.

Those responsibilities remain assigned to `HttpServer`, `HttpParser`, and
`HttpResponse`.

### Technical Decisions

- Introduced a dedicated `Router` class to separate request dispatch from TCP
  connection handling.

- Represented a handler as a callable receiving a parsed `HttpRequest` and
  returning an `HttpResponse`.

- Identified each route by the combination of an HTTP method and an exact path.

- Allowed the same path to be registered for different methods.

- Allowed the same method to be registered for different paths.

- Rejected route registrations containing:

  - an unsupported registration method;
  - an empty path;
  - a path that does not begin with `/`;
  - an empty handler;
  - a duplicate method-path combination.

- Added `freeze()` to prevent the route configuration from being modified after
  server initialization.

- Passed the configured router by value to the `HttpServer` constructor and
  moved it into the `router_` member.

- Froze the router during `HttpServer` construction so its configuration remains
  stable throughout request processing.

- Kept unknown methods syntactically valid in `HttpParser`. Whether a method is
  implemented is an application-level decision performed by `Router`.

- Applied the following routing decision order:

  1. an unknown method produces `501 Not Implemented`;
  2. a known method with an unknown path produces `404 Not Found`;
  3. an existing path without a handler for the requested method produces
     `405 Method Not Allowed`;
  4. an exact method-path match executes the registered handler.

- Made the unknown-method check occur before path lookup. Consequently, an
  unknown method used with an unknown path still produces
  `501 Not Implemented`.

- Added an `Allow` header to `405 Method Not Allowed` responses to indicate the
  methods registered for the requested path.

- Extended `HttpRequest` with a separate `query` member.

- Updated `HttpParser::parse()` to split the request-target at the first `?`
  character.

  For example:

  ```text
  /search?q=test
  ```

  is represented as:

  ```text
  path  = /search
  query = q=test
  ```

- Kept the query in its raw encoded form. Query decoding and parameter
  interpretation are not performed during routing.

- Used only the path when selecting a route. A query string therefore does not
  change which handler is selected.

- Updated `HttpServer::handleClient()` so that a successfully parsed request is
  passed to `Router::route()` instead of receiving a fixed response.

- Kept the current connection policy inside `HttpServer`. After routing,
  `HttpServer` adds `Connection: close`, serializes the response, transmits it
  with `sendAll()`, and closes the client socket.

### Problems Encountered and Fixes

- Constructing the response directly inside `handleClient()` mixed TCP connection
  management with application-level decisions. Routing was moved into a separate
  `Router` component.

- Matching directly on the complete request-target would have caused a request
  such as `/search?q=test` to differ from the registered `/search` route. The
  parser now separates the path from the query before routing.

- Looking up only a method-path combination was not sufficient to distinguish an
  unknown path from an existing path requested with the wrong method. The router
  now checks whether the path exists independently when determining whether to
  return `404` or `405`.

- An unknown method combined with an unknown path could have produced either
  `404` or `501` depending on the lookup order. The decision order was explicitly
  defined so unknown methods always produce `501`.

- Allowing route registration during request processing could make the routing
  configuration unpredictable. `freeze()` now prevents modifications after the
  server has taken ownership of the configured router.

- Registering the same method-path combination more than once could have silently
  replaced an existing handler. Duplicate registrations are now rejected.

### Accepted Simplifications

- Route matching is exact. Path parameters, wildcards, and prefix matching are
  not currently supported.

- The router recognizes a deliberately limited set of HTTP methods.

- Query strings do not participate in route selection.

- Query percent-decoding and parameter interpretation are not implemented.

- Handlers execute synchronously as part of the current sequential connection
  flow.

- Routing error responses use fixed status codes and response bodies.

- Middleware and grouped routes are not currently supported.

- The router cannot be modified after it has been frozen.

### Validation Evidence

- The complete project builds successfully with CMake.

- All 47 GoogleTest tests pass.

- The automated tests cover:

  - valid route registration;
  - rejection of invalid paths;
  - rejection of empty handlers;
  - rejection of unsupported registration methods;
  - rejection of duplicate method-path combinations;
  - registration of the same path for different methods;
  - registration of the same method for different paths;
  - rejection of registrations after `freeze()`;
  - dispatch to the correct handler;
  - `404 Not Found`;
  - `405 Method Not Allowed`;
  - generation of the `Allow` header;
  - `501 Not Implemented`;
  - separation of the path and query string;
  - routing independently of the query string.

- Manual end-to-end validation with `curl` covered five routing behaviors:

  1. a matching method and path execute the registered handler and return
     `200 OK`;
  2. a request containing a query string is routed using only its path;
  3. a known method with an unknown path returns `404 Not Found`;
  4. an existing path requested with another known method returns
     `405 Method Not Allowed` with an `Allow` header;
  5. an unknown method returns `501 Not Implemented`.

These validations exercise the complete request-response path:

```text
TCP reception
→ complete request reconstruction
→ HTTP parsing
→ path and query separation
→ route selection
→ handler or routing error response
→ response serialization
→ complete transmission
```

### What I Learned

- How to separate syntactic validation from application-level dispatch.
- How to represent a request handler with a callable object.
- Why a route is identified by both its method and its path.
- How to distinguish `404 Not Found` from `405 Method Not Allowed`.
- Why an unsupported method produces `501 Not Implemented`.
- Why the order of routing decisions affects the resulting HTTP status.
- How the `Allow` header communicates the methods supported by an existing
  resource.
- Why the query string must be separated from the path before route matching.
- How move semantics transfer a configured component into its owner.
- How freezing configuration creates a stable runtime state.
- How a routing layer keeps application logic separate from socket management.