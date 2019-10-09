#include <conversation.h>
#include <request.h>
#include <notify.h>
#include <util.h>
#include <eventloop.h>

#include "json_decode.hpp"

#include "purpleline.hpp"
#include "wrapper.hpp"

PINVerifier::PINVerifier(PurpleLine &parent) : parent(parent),
                                               http(parent.acct)
{
}

void PINVerifier::verify(
    line::LoginResult loginResult,
    std::function<void(std::string, std::string)> success)
{
    const int minutes = 3;

    const std::string verifier = loginResult.verifier;

    std::stringstream ss;
    ss
        << loginResult.pinCode
        << "\n\nThe number has to be entered into the LINE mobile application within "
        << minutes
        << " minutes. If the time runs out, reconnect to try again."
        << "\n\nYou will only have to verify your account once per computer.";
    std::string pin_msg = ss.str();

    notification = purple_request_action(
        (void *)parent.conn,
        "LINE account verification",
        "Enter this number on your mobile device",
        pin_msg.c_str(),
        0,
        parent.acct,
        nullptr,
        nullptr,
        (gpointer)this,
        1,
        "Cancel",
        (PurpleRequestActionCb)WRAPPER(PINVerifier::cancel_cb));

    timeout = purple_timeout_add_seconds(
        minutes * 60,
        WRAPPER(PINVerifier::timeout_cb),
        (gpointer)this);

    parent.set_auth_token(verifier);

    http.request(LINE_VERIFICATION_URL, HTTPFlag::AUTH,
                 [this, verifier, success](int status, const guchar *data, gsize len) {
                     if (!data || status != 200)
                     {
                         std::stringstream ss;
                         ss << "Account verification failed. Status: " << status;

                         error(ss.str());
                         return;
                     }

                     std::string json((const char *)data, len);

                     // Don't feel like invoking an entire JSON parser for this, this should be good enough.
                     if (json.find("\"QRCODE_VERIFIED\"") == std::string::npos)
                     {
                         error("Account verification failed: server would not verify.");
                         return;
                     }

                     // old verification
                     /*
                    parent.c_out->send_loginWithVerifierForCertificate(verifier);
                    parent.c_out->send([this, verifier, success]() {
                    line::LoginResult result;

                    try {
                        parent.c_out->recv_loginWithVerifierForCertificate(result);
                    } catch (line::TalkException &err) {
                        error("Account verification failed: Exception: " + err.reason);
                        return;
                    }
                    */

                   // JSON decode
                    nlohmann::json json_decoded = nlohmann::json::parse(json);

                    // Set Login Query URL
                    parent.c_out->set_path(LINE_LOGINZ_PATH);

                     line::LoginRequest req;
                     req.type = line::LoginType::QRCODE;
                     req.verifier = (std::string)json_decoded["result"]["verifier"];
                     req.e2eeVersion = 0;

                     parent.c_out->send_loginZ(req);
                     parent.c_out->send([this, verifier, success]() {
                         line::LoginResult req_result;

                         try
                         {
                             parent.c_out->recv_loginZ(req_result);
                         }
                         catch (line::TalkException &err)
                         {
                             error("Account verification failed: Exception: " + err.reason);

                             return;
                         }

                         if (req_result.authToken == "")
                         {
                             error("Account verification failed: no auth token.");
                             return;
                         }

                         end();

                         success(req_result.authToken, req_result.certificate);
                     });
                 });
}

int PINVerifier::timeout_cb()
{
    error("Account verification timed out.");

    return FALSE;
}

void PINVerifier::cancel_cb(int)
{
    error("Account verification cancelled.");
}

void PINVerifier::end()
{
    if (timeout)
    {
        purple_timeout_remove(timeout);
        timeout = 0;
    }

    if (notification)
    {
        purple_request_close(PURPLE_REQUEST_ACTION, notification);
        notification = nullptr;
    }
}

void PINVerifier::error(std::string error)
{
    end();
    purple_connection_error(parent.conn, error.c_str());
}
