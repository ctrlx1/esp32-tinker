#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace tinker {

class PortalRequest {
public:
  explicit PortalRequest(WebServer &server) : server_(server) {}

  String arg(const char *name) const { return server_.arg(name); }
  bool hasArg(const char *name) const { return server_.hasArg(name); }

private:
  WebServer &server_;
};

} // namespace tinker
