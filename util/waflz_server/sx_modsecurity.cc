//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "sx_modsecurity.h"
#include "waflz/waf.h"
#include "waflz/engine.h"
#include "waflz/profile.h"
#include "waflz/rqst_ctx.h"
#include "waflz/geoip2_mmdb.h"
#include "waflz/string_util.h"
#include "is2/support/trace.h"
#include "is2/support/nbq.h"
#include "is2/support/ndebug.h"
#include "is2/srvr/api_resp.h"
#include "is2/srvr/srvr.h"
#include "jspb/jspb.h"
#include "support/file_util.h"
#include "event.pb.h"
#include "action.pb.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
//! ----------------------------------------------------------------------------
//! constants
//! ----------------------------------------------------------------------------
#ifndef STATUS_OK
  #define STATUS_OK 0
#endif
#ifndef STATUS_ERROR
  #define STATUS_ERROR -1
#endif
#define _DEFAULT_RESP_BODY_B64 "PCFET0NUWVBFIGh0bWw+PGh0bWw+PGhlYWQ+IDxtZXRhIGNoYXJzZXQ9InV0Zi04Ij4gPHRpdGxlPjwvdGl0bGU+PC9oZWFkPjxib2R5PiA8c3R5bGU+Knstd2Via2l0LWJveC1zaXppbmc6IGJvcmRlci1ib3g7IC1tb3otYm94LXNpemluZzogYm9yZGVyLWJveDsgYm94LXNpemluZzogYm9yZGVyLWJveDt9ZGl2e2Rpc3BsYXk6IGJsb2NrO31ib2R5e2ZvbnQtZmFtaWx5OiAiSGVsdmV0aWNhIE5ldWUiLCBIZWx2ZXRpY2EsIEFyaWFsLCBzYW5zLXNlcmlmOyBmb250LXNpemU6IDE0cHg7IGxpbmUtaGVpZ2h0OiAxLjQyODU3MTQzOyBjb2xvcjogIzMzMzsgYmFja2dyb3VuZC1jb2xvcjogI2ZmZjt9aHRtbHtmb250LXNpemU6IDEwcHg7IC13ZWJraXQtdGFwLWhpZ2hsaWdodC1jb2xvcjogcmdiYSgwLCAwLCAwLCAwKTsgZm9udC1mYW1pbHk6IHNhbnMtc2VyaWY7IC13ZWJraXQtdGV4dC1zaXplLWFkanVzdDogMTAwJTsgLW1zLXRleHQtc2l6ZS1hZGp1c3Q6IDEwMCU7fTpiZWZvcmUsIDphZnRlcnstd2Via2l0LWJveC1zaXppbmc6IGJvcmRlci1ib3g7IC1tb3otYm94LXNpemluZzogYm9yZGVyLWJveDsgYm94LXNpemluZzogYm9yZGVyLWJveDt9LmNvbnRhaW5lcntwYWRkaW5nLXJpZ2h0OiAxNXB4OyBwYWRkaW5nLWxlZnQ6IDE1cHg7IG1hcmdpbi1yaWdodDogYXV0bzsgbWFyZ2luLWxlZnQ6IGF1dG87fUBtZWRpYSAobWluLXdpZHRoOiA3NjhweCl7LmNvbnRhaW5lcnt3aWR0aDogNzUwcHg7fX0uY2FsbG91dCsuY2FsbG91dHttYXJnaW4tdG9wOiAtNXB4O30uY2FsbG91dHtwYWRkaW5nOiAyMHB4OyBtYXJnaW46IDIwcHggMDsgYm9yZGVyOiAxcHggc29saWQgI2VlZTsgYm9yZGVyLWxlZnQtd2lkdGg6IDVweDsgYm9yZGVyLXJhZGl1czogM3B4O30uY2FsbG91dC1kYW5nZXJ7Ym9yZGVyLWxlZnQtY29sb3I6ICNmYTBlMWM7fS5jYWxsb3V0LWRhbmdlciBoNHtjb2xvcjogI2ZhMGUxYzt9LmNhbGxvdXQgaDR7bWFyZ2luLXRvcDogMDsgbWFyZ2luLWJvdHRvbTogNXB4O31oNCwgLmg0e2ZvbnQtc2l6ZTogMThweDt9aDQsIC5oNCwgaDUsIC5oNSwgaDYsIC5oNnttYXJnaW4tdG9wOiAxMHB4OyBtYXJnaW4tYm90dG9tOiAxMHB4O31oMSwgaDIsIGgzLCBoNCwgaDUsIGg2LCAuaDEsIC5oMiwgLmgzLCAuaDQsIC5oNSwgLmg2e2ZvbnQtZmFtaWx5OiBBcGV4LCAiSGVsdmV0aWNhIE5ldWUiLCBIZWx2ZXRpY2EsIEFyaWFsLCBzYW5zLXNlcmlmOyBmb250LXdlaWdodDogNDAwOyBsaW5lLWhlaWdodDogMS4xOyBjb2xvcjogaW5oZXJpdDt9aDR7ZGlzcGxheTogYmxvY2s7IC13ZWJraXQtbWFyZ2luLWJlZm9yZTogMS4zM2VtOyAtd2Via2l0LW1hcmdpbi1hZnRlcjogMS4zM2VtOyAtd2Via2l0LW1hcmdpbi1zdGFydDogMHB4OyAtd2Via2l0LW1hcmdpbi1lbmQ6IDBweDsgZm9udC13ZWlnaHQ6IGJvbGQ7fWxhYmVse2Rpc3BsYXk6IGlubGluZS1ibG9jazsgbWF4LXdpZHRoOiAxMDAlOyBtYXJnaW4tYm90dG9tOiA1cHg7IGZvbnQtd2VpZ2h0OiA3MDA7fWRse21hcmdpbi10b3A6IDA7IG1hcmdpbi1ib3R0b206IDIwcHg7IGRpc3BsYXk6IGJsb2NrOyAtd2Via2l0LW1hcmdpbi1iZWZvcmU6IDFlbTsgLXdlYmtpdC1tYXJnaW4tYWZ0ZXI6IDFlbTsgLXdlYmtpdC1tYXJnaW4tc3RhcnQ6IDBweDsgLXdlYmtpdC1tYXJnaW4tZW5kOiAwcHg7fWRke2Rpc3BsYXk6IGJsb2NrOyAtd2Via2l0LW1hcmdpbi1zdGFydDogNDBweDsgbWFyZ2luLWxlZnQ6IDA7IHdvcmQtd3JhcDogYnJlYWstd29yZDt9ZHR7Zm9udC13ZWlnaHQ6IDcwMDsgZGlzcGxheTogYmxvY2s7fWR0LCBkZHtsaW5lLWhlaWdodDogMS40Mjg1NzE0Mzt9LmRsLWhvcml6b250YWwgZHR7ZmxvYXQ6IGxlZnQ7IHdpZHRoOiAxNjBweDsgb3ZlcmZsb3c6IGhpZGRlbjsgY2xlYXI6IGxlZnQ7IHRleHQtYWxpZ246IHJpZ2h0OyB0ZXh0LW92ZXJmbG93OiBlbGxpcHNpczsgd2hpdGUtc3BhY2U6IG5vd3JhcDt9LmRsLWhvcml6b250YWwgZGR7bWFyZ2luLWxlZnQ6IDE4MHB4O308L3N0eWxlPiA8ZGl2IGNsYXNzPSJjb250YWluZXIiPiA8ZGl2IGNsYXNzPSJjYWxsb3V0IGNhbGxvdXQtZGFuZ2VyIj4gPGg0IGNsYXNzPSJsYWJlbCI+Rm9yYmlkZGVuPC9oND4gPGRsIGNsYXNzPSJkbC1ob3Jpem9udGFsIj4gPGR0PkNsaWVudCBJUDwvZHQ+IDxkZD57e0NMSUVOVF9JUH19PC9kZD4gPGR0PlVzZXItQWdlbnQ8L2R0PiA8ZGQ+e3tVU0VSX0FHRU5UfX08L2RkPiA8ZHQ+UmVxdWVzdCBVUkw8L2R0PiA8ZGQ+e3tSRVFVRVNUX1VSTH19PC9kZD4gPGR0PlJlYXNvbjwvZHQ+IDxkZD57e1JVTEVfTVNHfX08L2RkPiA8ZHQ+RGF0ZTwvZHQ+IDxkZD57e1RJTUVTVEFNUH19PC9kZD4gPC9kbD4gPC9kaXY+PC9kaXY+PC9ib2R5PjwvaHRtbD4="
namespace ns_waflz_server {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
sx_modsecurity::sx_modsecurity(ns_waflz::engine& a_engine):
        m_engine(a_engine),
        m_waf(NULL),
        m_geoip2_mmdb(NULL),
        m_action(NULL)
{
        // -------------------------------------------------
        // set up default enforcement
        // -------------------------------------------------
        m_action = new waflz_pb::enforcement();
        m_action->set_type("CUSTOM_RESPONSE");
        m_action->set_enf_type(waflz_pb::enforcement_type_t_BLOCK_REQUEST);
        m_action->set_status(403);
        m_action->set_response_body_base64(_DEFAULT_RESP_BODY_B64);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
sx_modsecurity::~sx_modsecurity(void)
{
        if(m_waf) { delete m_waf; m_waf = NULL; }
        if(m_geoip2_mmdb) { delete m_geoip2_mmdb; m_geoip2_mmdb = NULL; }
        if(m_action) { delete m_action; m_action = NULL; }
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t sx_modsecurity::init(void)
{
        int32_t l_s;
        // -------------------------------------------------
        // guess format from ext...
        // -------------------------------------------------
        ns_waflz::config_parser::format_t l_fmt = ns_waflz::config_parser::MODSECURITY;
        std::string l_ext;
        l_ext = ns_waflz::get_file_ext(m_config);
        if(l_ext == "json")
        {
                l_fmt = ns_waflz::config_parser::JSON;
        }
        // -------------------------------------------------
        // make waf obj
        // -------------------------------------------------
        m_waf = new ns_waflz::waf(m_engine);
        l_s = m_waf->init(l_fmt, m_config, true);
        if(l_s != WAFLZ_STATUS_OK)
        {
                NDBG_PRINT("error loading conf file: %s. reason: %s\n",
                           m_config.c_str(),
                           "__na__");
                           // TODO -get reason...
                           //l_wafl->get_err_msg());
                return STATUS_ERROR;
        }
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
ns_is2::h_resp_t sx_modsecurity::handle_rqst(waflz_pb::enforcement **ao_enf,
                                             ns_waflz::rqst_ctx **ao_ctx,
                                             ns_is2::session &a_session,
                                             ns_is2::rqst &a_rqst,
                                             const ns_is2::url_pmap_t &a_url_pmap)
{
        if(ao_enf) { *ao_enf = NULL;}
        m_resp = "{\"status\": \"ok\"}";
        if(!m_waf)
        {
                return ns_is2::H_RESP_SERVER_ERROR;
        }
        int32_t l_s;
        ns_waflz::rqst_ctx *l_ctx = NULL;
        waflz_pb::event *l_event = NULL;
        l_ctx = new ns_waflz::rqst_ctx((void *)&a_session, DEFAULT_BODY_SIZE_MAX, m_callbacks, false, false);
        // -------------------------------------------------
        // process
        // -------------------------------------------------
        l_s = m_waf->process(&l_event, &a_session, &l_ctx);
        if(l_s != WAFLZ_STATUS_OK)
        {
                NDBG_PRINT("error processing config. reason. TBD\n");
                if(l_event) { delete l_event; l_event = NULL; }
                if(l_ctx) { delete l_ctx; l_ctx = NULL; }
                return ns_is2::H_RESP_SERVER_ERROR;
        }
        if(!l_event)
        {
                if(l_ctx) { delete l_ctx; l_ctx = NULL; }
                return ns_is2::H_RESP_DONE;
        }
        l_ctx->m_event = l_event;
        // -------------------------------------------------
        // serialize event...
        // -------------------------------------------------
        l_s = ns_waflz::convert_to_json(m_resp, *l_event);
        if(l_s != JSPB_OK)
        {
                NDBG_PRINT("error performing convert_to_json.\n");
                if(l_event) { delete l_event; l_event = NULL; }
                if(l_ctx) { delete l_ctx; l_ctx = NULL; }
                return ns_is2::H_RESP_SERVER_ERROR;
        }
        // -------------------------------------------------
        // create enforcement copy...
        // -------------------------------------------------
        if(m_action)
        {
                *ao_enf = new waflz_pb::enforcement();
                (*ao_enf)->CopyFrom(*m_action);
        }
        // -------------------------------------------------
        // cleanup
        // -------------------------------------------------
        if(ao_ctx)
        {
                *ao_ctx = l_ctx;
        }
        else if(l_ctx)
        {
                delete l_ctx; l_ctx = NULL;
        }
        return ns_is2::H_RESP_DONE;
}
}
