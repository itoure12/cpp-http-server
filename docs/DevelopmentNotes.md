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

> Status: in progress
> Part 1 completed: `HttpParser`
> Part 2 in progress: TCP reception in `HttpServer::handleClient()`

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

- All 21 GoogleTest tests pass.
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

### Part 2 — TCP Reception in handleClient

#### Current State

- `handleClient()` is declared, defined, and called by `acceptLoop()`.
- An initial read using `recv()` has been implemented.
- The request sent by `curl` is received and displayed correctly.
- The client socket is closed after it is handled.
- No HTTP response is sent yet.

#### Current Validation

A local request sent with:

```bash
curl http://localhost:8080/
```

is received and displayed by the server.

`curl` currently reports the `Empty reply from server` error. This behavior is
expected: the server closes the client connection without sending an HTTP
response because `handleClient()` does not call `send()` yet.

#### Remaining Work

- Receive a request that may be fragmented across multiple calls to `recv()`.
- Accumulate only the bytes actually received.
- Detect the end of the headers with `\r\n\r\n`.
- Determine the expected body size from `Content-Length`.
- Enforce size limits for headers and the body.
- Detect disconnections and receive errors.
- Call `HttpParser::parse()` once the complete request has been received.
- Handle a rejected request cleanly.
- Add tests for fragmented reception.
- Prepare for future HTTP response generation.