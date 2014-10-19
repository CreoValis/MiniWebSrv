# MiniWebSrv library

MiniWebSrv is a small, embeddable HTTPd library and a demonstration application
implementing a small HTTP server.

The library is intended to be used as an external interface for applications
whose primary purpose is not to be a general HTTP server. Because of this, the
library is not, and does not strive to be, a complete implementation of
HTTP/1.1 . It only supports a subset of the specification which allows common
clients (Firefox, Chrome, curl, etc.) to interact with it.

## Features
The library provides the following features:

* Separate worker thread: all requests are served on a single worker thread.
* A connection filter interface to accept or reject incoming connections based
on the source IP address. Two built-in filters are provided: allow-all and
loopback-only.
* A log interface to trace connections and requests.
* Basic HTTP/1.1 support: keepalive connections, with single-request
connections for HTTP/1.0 or on request.
* GET and POST query parameter parser, with file upload support.
* Fully customizable response generators, with a few built-in:
  * Static file serving with last modification date support.
  * Static file serving from zip archives (blindly expects user agents to
  support gzip content-encoding).
* Easy interface to generate custom responses.
* WebSocket support with simple interfaces to implement.

## Documentation
TODO :)

## Supported platforms
 * Windows 7
 * Linux
 * (probably every platform where the required boost libraries are supported)

## Compiling
The following are required to compile the library and the application:

* A C++11 compiler. Tested with the following:
  * MSVC 11
  * LLVM clang 3.5
  * GCC 4.8 (4.7 would probably work for now)
* The Boost C++ libraries.
  * v1.55 and v1.56 are known to work.
  * The following separately compiled libraries are required:
     * boost\_system
     * boost\_filesystem (required only by the static file response generator)
     * boost\_thread
     * boost\_coroutine
     * boost\_chrono
