#pragma once
#include <WString.h>

// Whether a browser request came from one of this controller's own pages.
//
// Anything a page sends to another address — a form post, a WebSocket — goes
// with an Origin header naming the page it came from, and other websites can
// send both. Only http://<name>.local and http://<its IP address> count as
// ours: those are the addresses the README and the serial log give out.
// Checking against them, rather than against whatever host the request was
// sent to, also turns away a website that has pointed its own domain name at
// this controller's IP.
//
// A request with no Origin at all isn't from a web page (a panel, curl), and
// is let through by the callers: that's someone on the network already.
bool isOwnOrigin(const String& origin, const char* name);

// The same test for the address a page was loaded from (the Host header), so
// a page opened at some other address can say where saving and the controls
// do work.
bool isOwnHost(const String& host, const char* name);
