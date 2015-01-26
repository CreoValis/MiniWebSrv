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
  * Static file serving from zip archives last modification date support
  (blindly expects user agents to support gzip content-encoding).
  * Common error response generator (generates error pages from http error
  codes or std::exception objects)
* Easy interface to generate custom responses.
* WebSocket support with simple interfaces to implement.

## Example
This simple example serves files straight from a zip file. Accesses are logged
to stdout.

```c++
#include <iostream>
#include "HTTP/Server.h"
#include "HTTP/RespSources/ZipRespSource.h"
#include "HTTP/ServerLogs/OStreamServerLog.h"

int main(int argc, char **argv)
{
	//Create a server object to listen on *:80 .
	HTTP::Server MiniWS(80);
	//Create and set a ResponseSource, which will serve files from "test.zip".
	MiniWS.SetResponseSource(new HTTP::RespSource::Zip("test.zip"));
	//Create and set the server log object, with std::cout as it's target.
	MiniWS.SetServerLog(new HTTP::ServerLog::OStream(std::cout));

	//Start the background thread and open the listener socket.
	MiniWS.Run();
	//...
	//Stop the server, allowing 4 seconds for the background thread to exit.
	MiniWS.Stop(boost::posix_time::seconds(4));

	return 0;
}
```

For a more complete example including a custom ResponseSource demonstrating the
use of query parameters and file uploads, see
[MiniWebSrv.cpp](MiniWebSrv/MiniWebSrv.cpp).

## Documentation
Currently, the repository contains the library and a simple demonstration
application. These are only logically separated: the library does not build
to a separate object.

### Project layout
The layout is as follows:

* The [MiniWebSrv/HTTP](MiniWebSrv/HTTP) directory contains the HTTPd library
in it's entirety. Copying the files from here and dropping them into your
project is the easiest way to use it.
* The [iniWebSrv/MiniWebSrv.cpp](MiniWebSrv/MiniWebSrv.cpp) file contains th
demonstration application. It shows the basic steps to configure, start and
stop the HTTPd server.

### Basic architecture
The central class in the library is `HTTP::Server`. This class contains the
single thread used by the HTTPd library to run the protocol parser and create
responses. It also listens to incoming connection requests, maintains the list
of active Connection objects, and destroys them when needed. It also contains
the main customizable objects of the library: the connection filter, response
source and server log.

Connection filters are derived from `HTTP::IConnFilter`. Objects of this type
are used to determine if a given client can connect or not. Though the
interface contains a method which can filter on resources, this is not yet
used.

Response sources are derived from `HTTP::IRespSource`. These are the main
workhorses of the library. Instances of this class can create `HTTP::IResponse`
derived objects, which define the contents of the response.

The server log object receives method calls for each connection attempt, HTTP
request and websocket connection. These classes are derived from
`HTTP::IServerLog`.

### Configuration
Some basic parts of the library can be configured in
[HTTP/BuildConfig.h](MiniWebSrv/HTTP/BuildConfig.h) by modifying the constants
defined there. These include buffer sizes and silent connection timeout.

### Websocket support
Websocket connections are supported through the HTTP connection upgrade
process, as specified by the standard. The library's architecture mirrors this
process.

After a request was processed, and the response was generated and fully sent,
the `HTTP::IResponse` object has a chance to upgrade the current connection. If
it chooses to do so, it returns a new connection object, which should replace
the current one in the `HTTP::Server` object's connection list.

Websocket support is implemented through this facility. Websocket upgrade
responses are served by an IRespSource: derivatives of the
`HTTP::WebSocket::WSRespSource` class. The derived class should override a
specific method, which can return a `HTTP::WebSocket::IMsgHandler` object.
This will receive an object to send websocket messages with, and it will
receive method calls when an incoming message arrives.
The `HTTP::WebSocket::WSRespSource` class creates a simple IResponse object,
which handles the upgrade process. The connection it upgrades to contains the
websocket protocol handler code.

The websocket connection class (`HTTP::WebSocket::Connection`) supports the
whole specification, though without any extensions. It places artificial limits
on the incoming frames and assembled messages, which can be configured in
[HTTP/BuildConfig.h](MiniWebSrv/HTTP/BuildConfig.h).

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
