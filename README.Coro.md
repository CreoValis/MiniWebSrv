# MiniWebSrv: coroutine-based request handler support

## API overview

The response objects in MiniWebSrv (`IResponse`) implement a pull-style API for generating the response body. In some
cases, this is inconvenient, and push-style operation would be preferable.

MiniWebSrv contains a response class for this case, called `CoroResponse`. It can be created by specifying a callable
(possibly a lambda function), which will be run in a new coroutine created with `boost::context`.

First, this function should set up the response's parameters (http status code, response length, headers) using a
helper object (`CoroResponse::ResponseParams`). After these are set up, the function should use an "output stream"
object (`CoroResponse::OutStream`) to generate response data to send. This object can be used multiple times, with each
`Write` call sending the specified number of bytes to the client. After the full response has been generated, the
function should return normally. This finishes HTTP response.

## Example usage

The following example creates a CoroResponse, which sets up a simple text response, then sends 6 bytes to the client in
two calls:

````cpp
using HTTP::RespSource;

new CoroResponse([](const GenericBase::CallParams &CParams, ResponseParams &RParams, OutStream &Stream) {
	//Set up some basic response parameters.
	RParams.SetResponseLength(6);
	RParams.SetContentType("text/plain");
	//Send the response to the client. This suspends the response coroutine, until the response body can be sent.
	RParams.Finalize(Stream);

	//Copy 3 bytes to the response buffer...
	memcpy(Stream.GetBuff(), "abc", 3);
	//...then send it to the client.
	Stream.Write(3);

	memcpy(Stream.GetBuff(), "def", 3);
	Stream.Write(3);

	//After this function returns, the HTTP response is considered finished.
});
````

The `make_coro_respsource` helper function can be used to a create an IRespSource from a lambda:

````cpp
using HTTP::RespSource;

auto RespSource=HTTP::RespSource::make_coro_respsource(
	[](const GenericBase::CallParams &CParams, CoroResponse::ResponseParams &RParams, CoroResponse::OutStream &OutS) {
	RParams.SetResponseLength(6);
	RParams.SetContentType("text/plain");
	RParams.Finalize(Stream);

	memcpy(Stream.GetBuff(), "ghijkl", 6);
	Stream.Write(3);
});
````
