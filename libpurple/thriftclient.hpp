#pragma once

#include <string>
#include <deque>

#include <debug.h>
#include <plugin.h>
#include <prpl.h>

#include "thrift_line/TalkService.h"

#include "linehttptransport.hpp"

class ThriftClient : public line::TalkServiceClient {

    std::string path;
    std::shared_ptr<LineHttpTransport> http;

public:

    ThriftClient(PurpleAccount *acct, PurpleConnection *conn, std::string path);

    void set_path(std::string path);
    void set_auto_reconnect(bool auto_reconnect);
    void send(std::function<void()> callback);

    int status_code();
    void close();

};
