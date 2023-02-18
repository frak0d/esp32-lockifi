#pragma once

extern "C"
{
    #include <esp_err.h>
    #include <esp_http_server.h>
}

namespace Api
{

esp_err_t get_example(httpd_req_t *req)
{
    /* Send a simple response */
    const char resp[] = "URI GET Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t post_example(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = 5;//MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    /* Send a simple response */
    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t ping_fn(httpd_req_t* req)
{
    return httpd_resp_send(req, "UwU", HTTPD_RESP_USE_STRLEN);
}

esp_err_t access_logs_fn(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t user_list_fn(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t add_user_fn(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t remove_user_fn(httpd_req_t* req)
{
    return ESP_OK;
}

esp_err_t check_user_fn(httpd_req_t* req)
{
    return ESP_OK;
}

/////////////////////////////////////////////

httpd_uri_t ping
{
    .uri      = "/ping",
    .method   = HTTP_GET,
    .handler  = Api::ping_fn
};
httpd_uri_t access_logs
{
    .uri      = "/access_logs",
    .method   = HTTP_GET,
    .handler  = access_logs_fn
};
httpd_uri_t user_list
{
    .uri      = "/user_list",
    .method   = HTTP_GET,
    .handler  = user_list_fn
};
httpd_uri_t add_user
{
    .uri      = "/add_user",
    .method   = HTTP_POST,
    .handler  = add_user_fn
};
httpd_uri_t remove_user
{
    .uri      = "/remove_user",
    .method   = HTTP_POST,
    .handler  = remove_user_fn
};
httpd_uri_t check_user
{
    .uri      = "/check_user",
    .method   = HTTP_POST,
    .handler  = check_user_fn
};

}
