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
#include "waflz/def.h"
#include "waflz/config_parser.h"
#include "waflz/engine.h"
#include "waflz/geoip2_mmdb.h"
#include "waflz/string_util.h"
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/error/en.h>
#include "support/file_util.h"
#include "support/ndebug.h"
#include "op/regex.h"
#include "op/ac.h"
#include "op/nms.h"
#include "op/byte_range.h"
#include "core/macro.h"
#include "core/tx.h"
#include "core/var.h"
#include "core/op.h"
#include <libxml/parser.h>
namespace ns_waflz {
//! ----------------------------------------------------------------------------
//! statics
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details define error function for libxml2 to "| > /dev/null" errors
//!          defined w/ xmlGenericErrorFunc function sig
//! \return  NA
//! ----------------------------------------------------------------------------
static void xml_error_func(void* ctx, const char* msg, ...)
{
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
_compiled_config::~_compiled_config()
{
        // -------------------------------------------------
        // destruct m_regex_list
        // -------------------------------------------------
        for (regex_list_t::iterator i_p = m_regex_list.begin();
            i_p != m_regex_list.end();
            ++i_p)
        {
                if (*i_p) { delete *i_p; *i_p = NULL;}
        }
        // -------------------------------------------------
        // destruct m_nms_list
        // -------------------------------------------------
        for (nms_list_t::iterator i_n = m_nms_list.begin();
            i_n != m_nms_list.end();
            ++i_n)
        {
                if (*i_n) { delete *i_n; *i_n = NULL;}
        }
        // -------------------------------------------------
        // destruct m_ac_list
        // -------------------------------------------------
        for (ac_list_t::iterator i_p = m_ac_list.begin();
            i_p != m_ac_list.end();
            ++i_p)
        {
                if (*i_p) { delete *i_p; *i_p = NULL;}
        }
        // -------------------------------------------------
        // destruct m_byte_range_list
        // -------------------------------------------------
        for (byte_range_list_t::iterator i_p = m_byte_range_list.begin();
            i_p != m_byte_range_list.end();
            ++i_p)
        {
                if (*i_p) { delete *i_p; *i_p = NULL;}
        }
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
engine::engine():
        m_macro(NULL),
        m_config_list(),
        m_compiled_config_map(),
        m_ctype_parser_map(),
        m_ruleset_root_dir("/oc/local/waf/ruleset/"),
        m_geoip2_mmdb(),
        m_geoip2_db(),
        m_geoip2_isp_db(),
        m_err_msg()
{
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
engine::~engine()
{
        // -------------------------------------------------
        // destruct m_config_list
        // -------------------------------------------------
        for (config_list_t::iterator i_cfg = m_config_list.begin();
            i_cfg != m_config_list.end();
            ++i_cfg)
        {
                if (*i_cfg) { delete *i_cfg; *i_cfg = NULL;}
        }
        // -------------------------------------------------
        // destruct m_compiled_config_map
        // -------------------------------------------------
        for (compiled_config_map_t::iterator i_cfg = m_compiled_config_map.begin();
            i_cfg != m_compiled_config_map.end();
            ++i_cfg)
        {
                if (i_cfg->second) { delete i_cfg->second; i_cfg->second = NULL;}
        }
        // -------------------------------------------------
        // *************************************************
        //             xml initialization
        // *************************************************
        // -------------------------------------------------
        xmlCleanupParser();
        if (m_macro)
        {
                delete m_macro;
                m_macro = NULL;
        }
        // -------------------------------------------------
        // *************************************************
        //                  geoip2 dbs
        // *************************************************
        // -------------------------------------------------
        if (m_geoip2_mmdb)
        {
                delete m_geoip2_mmdb;
                m_geoip2_mmdb = NULL;
        }
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t engine::init()
{
        int32_t l_s;
        // -------------------------------------------------
        // 
        // -------------------------------------------------

        // -------------------------------------------------
        // macro...
        // -------------------------------------------------
        if (m_macro)
        {
                delete m_macro;
                m_macro = NULL;
        }
        m_macro = new macro();
        l_s = m_macro->init();
        if (l_s != WAFLZ_STATUS_OK)
        {
                return WAFLZ_STATUS_ERROR;
        }
        // -------------------------------------------------
        // init...
        // -------------------------------------------------
        init_tx_cb_vector();
        init_var_cb_vector();
        init_resp_var_cb_vector();
        init_op_cb_vector();
        // -------------------------------------------------
        // *************************************************
        //         Content-Type --> parser map
        // *************************************************
        // -------------------------------------------------
        m_ctype_parser_map["application/x-www-form-urlencoded"] = PARSER_URL_ENCODED;
        m_ctype_parser_map["text/xml"]                          = PARSER_XML;
        m_ctype_parser_map["application/xml"]                   = PARSER_XML;
        m_ctype_parser_map["application/json"]                  = PARSER_JSON;
        m_ctype_parser_map["multipart/form-data"]               = PARSER_MULTIPART;
        // -------------------------------------------------
        // *************************************************
        //             xml initialization
        // *************************************************
        // -------------------------------------------------
        xmlGenericErrorFunc l_err_h = (xmlGenericErrorFunc)xml_error_func;
        initGenericErrorDefaultFunc(&l_err_h);
        xmlInitParser();
        // -------------------------------------------------
        // *************************************************
        //                  geoip2 dbs
        // *************************************************
        // -------------------------------------------------
        m_geoip2_mmdb = new geoip2_mmdb();
        l_s = m_geoip2_mmdb->init(m_geoip2_db, m_geoip2_isp_db);
        if (l_s != WAFLZ_STATUS_OK)
        {
                 WAFLZ_PERROR(m_err_msg,"error intializing");
                 return WAFLZ_STATUS_OK;
        }
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: compile regexes in vars
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t engine::compile(compiled_config_t& ao_cx_cfg,
                        waflz_pb::sec_config_t& a_config,
                        const std::string& a_ruleset_dir)
{
        // -------------------------------------------------
        // clear all
        // -------------------------------------------------
        ao_cx_cfg.m_marker_map_phase_1.clear();
        ao_cx_cfg.m_marker_map_phase_2.clear();
        ao_cx_cfg.m_marker_map_phase_3.clear();
        ao_cx_cfg.m_marker_map_phase_4.clear();
        ao_cx_cfg.m_directive_list_phase_1.clear();
        ao_cx_cfg.m_directive_list_phase_2.clear();
        ao_cx_cfg.m_directive_list_phase_3.clear();
        ao_cx_cfg.m_directive_list_phase_4.clear();
        // -------------------------------------------------
        // for each directive...
        // -------------------------------------------------
        for (int i_d = 0; i_d < a_config.directive_size(); ++i_d)
        {
                const ::waflz_pb::directive_t& l_d = a_config.directive(i_d);
                // -----------------------------------------
                // include
                // -----------------------------------------
                if (l_d.has_include())
                {
                        int32_t l_s;
                        compiled_config_t* l_cx_cfg = NULL;
                        l_s = process_include(&l_cx_cfg,
                                              l_d.include(),
                                              a_config,
                                              a_ruleset_dir);
                        if (l_s != WAFLZ_STATUS_OK)
                        {
                                // TODO log reason???
                                return WAFLZ_STATUS_ERROR;
                        }
                        if (!l_cx_cfg)
                        {
                                continue;
                        }
                        l_s = merge(ao_cx_cfg, *l_cx_cfg);
                        if (l_s != WAFLZ_STATUS_OK)
                        {
                                // TODO log reason???
                                return WAFLZ_STATUS_ERROR;
                        }
                        continue;
                }
                // -----------------------------------------
                // marker
                // -----------------------------------------
                if (l_d.has_marker())
                {
                        ao_cx_cfg.m_directive_list_phase_1.push_back(&l_d);
                        ao_cx_cfg.m_directive_list_phase_2.push_back(&l_d);
                        ao_cx_cfg.m_directive_list_phase_3.push_back(&l_d);
                        ao_cx_cfg.m_directive_list_phase_4.push_back(&l_d);
                        const ::std::string& l_m = l_d.marker();
                        ao_cx_cfg.m_marker_map_phase_1[l_m] = --(ao_cx_cfg.m_directive_list_phase_1.end());
                        ao_cx_cfg.m_marker_map_phase_2[l_m] = --(ao_cx_cfg.m_directive_list_phase_2.end());
                        ao_cx_cfg.m_marker_map_phase_3[l_m] = --(ao_cx_cfg.m_directive_list_phase_3.end());
                        ao_cx_cfg.m_marker_map_phase_4[l_m] = --(ao_cx_cfg.m_directive_list_phase_4.end());
                        continue;
                }
                // -----------------------------------------
                // action
                // -----------------------------------------
                if (l_d.has_sec_action())
                {
                        const waflz_pb::sec_action_t& l_a = l_d.sec_action();
                        if (!l_a.has_phase())
                        {
                                continue;
                        }
                        if (l_a.phase() == 1)
                        {
                                ao_cx_cfg.m_directive_list_phase_1.push_back(&l_d);
                        }
                        if (l_a.phase() == 2)
                        {
                                ao_cx_cfg.m_directive_list_phase_2.push_back(&l_d);
                        }
                        if(l_a.phase() == 3)
                        {
                                ao_cx_cfg.m_directive_list_phase_3.push_back(&l_d);
                        }
                        if(l_a.phase() == 4)
                        {
                                ao_cx_cfg.m_directive_list_phase_4.push_back(&l_d);
                        }

                }
                // -----------------------------------------
                // rule processing
                // -----------------------------------------
                if (!l_d.has_sec_rule())
                {
                        continue;
                }
                ::waflz_pb::directive_t& l_md = *(a_config.mutable_directive(i_d));
                waflz_pb::sec_rule_t& l_r = *(l_md.mutable_sec_rule());
                if (!l_r.has_action())
                {
                        continue;
                }
                const waflz_pb::sec_action_t& l_a = l_r.action();
                // -----------------------------------------
                // check for missing msg
                // -----------------------------------------
                bool l_is_missing_msg = false;
                if (!l_a.has_msg())
                {
                        l_r.mutable_action()->set_msg("__na__");
                        l_is_missing_msg = true;
                }
                if (l_a.phase() == 1)
                {
                        ao_cx_cfg.m_directive_list_phase_1.push_back(&l_d);
                }
                if (l_a.phase() == 2)
                {
                        ao_cx_cfg.m_directive_list_phase_2.push_back(&l_d);
                }
                if(l_a.phase() == 3)
                {
                        ao_cx_cfg.m_directive_list_phase_3.push_back(&l_d);
                }
                if(l_a.phase() == 4)
                {
                        ao_cx_cfg.m_directive_list_phase_4.push_back(&l_d);
                }
                waflz_pb::sec_rule_t *l_rule = &l_r;
                bool l_is_block = false;
                bool l_has_anomaly_score = false;
                bool l_has_severity = false;
                int32_t l_cr_idx = -1;
                do {
                        if ((l_cr_idx >= 0) &&
                           (l_cr_idx < l_r.chained_rule_size()))
                        {
                                l_rule = l_r.mutable_chained_rule(l_cr_idx);
                        }
                        // ---------------------------------
                        // action
                        // ---------------------------------
                        if (l_rule->has_action())
                        {
                                const ::waflz_pb::sec_action_t& l_a = l_rule->action();
                                // -------------------------
                                // check for block
                                // -------------------------
                                if (l_a.has_action_type() &&
                                                l_a.action_type() == ::waflz_pb::sec_action_t_action_type_t_BLOCK)
                                {
                                        l_is_block = true;
                                }
                                // -------------------------
                                // severity
                                // -------------------------
                                if (l_a.has_severity())
                                {
                                        l_has_severity = true;
                                }
                                // -------------------------
                                // check for anomaly...
                                // -------------------------
                                for (int32_t i_s = 0; i_s < l_a.setvar_size(); ++i_s)
                                {
                                        if (l_a.setvar(i_s).has_scope() &&
                                           (l_a.setvar(i_s).scope() == ::waflz_pb::sec_action_t_setvar_t_scope_t_TX) &&
                                           l_a.setvar(i_s).has_var() &&
                                           (strcasecmp(l_a.setvar(i_s).var().c_str(), "anomaly_score") == 0) &&
                                           l_a.setvar(i_s).has_op() &&
                                           l_a.setvar(i_s).op() == ::waflz_pb::sec_action_t_setvar_t_op_t_INCREMENT)
                                        {
                                                l_has_anomaly_score = true;
                                                break;
                                        }
                                }
                        }
                        // ---------------------------------
                        // fix missing message
                        // ---------------------------------
                        if (l_is_missing_msg &&
                           l_rule->has_action() &&
                           l_rule->action().has_msg())
                        {
                                l_r.mutable_action()->set_msg(l_rule->action().msg());
                                l_is_missing_msg = false;
                        }
                        // ---------------------------------
                        // var loop
                        // ---------------------------------
                        for (int32_t i_var = 0; i_var < l_rule->variable_size(); ++i_var)
                        {
                                waflz_pb::variable_t& l_var = *(l_rule->mutable_variable(i_var));
                                if (!l_var.has_type())
                                {
                                        continue;
                                }
                                // -------------------------
                                // *************************
                                // match
                                // *************************
                                // -------------------------
                                for (int32_t i_m = 0; i_m < l_var.match_size(); ++i_m)
                                {
                                        ::waflz_pb::variable_t_match_t& l_match = *(l_var.mutable_match(i_m));
                                        if (!l_match.is_regex() ||
                                           !l_match.has_value() ||
                                           l_match.value().empty())
                                        {
                                                continue;
                                        }
                                        int32_t l_s;
                                        regex* l_pcre = NULL;
                                        l_pcre = new regex();
                                        const std::string& l_rx = l_match.value();
                                        l_s = l_pcre->init(l_rx.c_str(), l_rx.length());
                                        if (l_s != WAFLZ_STATUS_OK)
                                        {
                                                const char* l_err_ptr;
                                                int l_err_off;
                                                l_pcre->get_err_info(&l_err_ptr, l_err_off);
                                                WAFLZ_PERROR(m_err_msg, "Failed to init re from %s. Reason: %s -offset: %d",
                                                             l_rx.c_str(),
                                                             l_err_ptr,
                                                             l_err_off);
                                                if (l_pcre) { delete l_pcre; l_pcre = NULL; }
                                                return WAFLZ_STATUS_ERROR;
                                        }
                                        ao_cx_cfg.m_regex_list.push_back(l_pcre);
                                        l_match.set__reserved_1((uint64_t)l_pcre);
                                }
                        }

                        // ---------------------------------
                        // *********************************
                        // operator
                        // *********************************
                        // ---------------------------------
                        if (!l_rule->has_operator_() ||
                           !l_rule->operator_().has_type() ||
                           !l_rule->operator_().has_value())
                        {
                                ++l_cr_idx;
                                continue;
                        }
                        ::waflz_pb::sec_rule_t_operator_t_type_t l_op_t = l_rule->operator_().type();
                        switch(l_op_t)
                        {
                        // ---------------------------------
                        // IPMATCH
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_IPMATCH:
                        {
                                int32_t l_s;
                                nms* l_nms = NULL;
                                l_s = create_nms_from_str(&l_nms, l_rule->operator_().value());
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        WAFLZ_PERROR(m_err_msg, "Failed to create nms from str %s", l_rule->operator_().value().c_str());
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_nms_list.push_back(l_nms);
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_nms);
                                break;
                        }
                        // ---------------------------------
                        // PM FROM FILE
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_IPMATCHF:
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_IPMATCHFROMFILE:
                        {
                                int32_t l_s;
                                nms* l_nms = NULL;
                                std::string l_f_path = a_ruleset_dir;
                                l_f_path.append(l_rule->operator_().value());
                                l_s = create_nms_from_file(&l_nms, l_f_path);
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        WAFLZ_PERROR(m_err_msg, "Failed to create nms from file %s %s", 
							l_rule->operator_().value().c_str(), strerror(errno));
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_nms_list.push_back(l_nms);
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_nms);
                                break;
                        }
                        // ---------------------------------
                        // PM
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_PM:
                        {
                                int32_t l_s;
                                ac* l_ac = NULL;
                                l_s = create_ac_from_str(&l_ac, l_rule->operator_().value());
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        WAFLZ_PERROR(m_err_msg, "Failed to create ac from string %s %s", 
							l_rule->operator_().value().c_str(), strerror(errno));
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_ac_list.push_back(l_ac);
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_ac);
                                break;
                        }
                        // ---------------------------------
                        // PM FROM FILE
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_PMF:
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_PMFROMFILE:
                        {
                                int32_t l_s;
                                ac* l_ac = NULL;
                                std::string l_f_path = a_ruleset_dir;
                                l_f_path.append(l_rule->operator_().value());
                                l_s = create_ac_from_file(&l_ac, l_f_path);
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        WAFLZ_PERROR(m_err_msg, "Failed to create ac from file %s %s", 
							l_rule->operator_().value().c_str(), strerror(errno));
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_ac_list.push_back(l_ac);
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_ac);
                                break;
                        }
                        // ---------------------------------
                        // RX
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_RX:
                        {
                                int32_t l_s;
                                regex* l_pcre = NULL;
                                l_pcre = new regex();
                                const std::string& l_rx = l_rule->operator_().value();
                                // -------------------------
                                // XXX -special exception for a single
                                // rx using a macro expansion
                                // -------------------------
                                if (l_rx == "^%{tx.allowed_request_content_type}$")
                                {
                                        l_rule->mutable_operator_()->set_type(::waflz_pb::sec_rule_t_operator_t_type_t_WITHIN);
                                        l_rule->mutable_operator_()->set_value("%{tx.allowed_request_content_type}");
                                        if (l_pcre) { delete l_pcre; l_pcre = NULL; }
                                        break;
                                }
                                l_s = l_pcre->init(l_rx.c_str(), l_rx.length());
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        const char* l_err_ptr;
                                        int l_err_off;
                                        l_pcre->get_err_info(&l_err_ptr, l_err_off);
                                        WAFLZ_PERROR(m_err_msg, "Failed to init re from %s. Reason: %s -offset: %d",
                                                     l_rx.c_str(),
                                                     l_err_ptr,
                                                     l_err_off);
                                        if (l_pcre) { delete l_pcre; l_pcre = NULL; }
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_regex_list.push_back(l_pcre);;
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_pcre);
                                break;
                        }
                        // ---------------------------------
                        // VERIFYCC
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_VERIFYCC:
                        {
                                int32_t l_s;
                                regex* l_pcre = NULL;
                                l_pcre = new regex();
                                const std::string& l_rx = l_rule->operator_().value();
                                l_s = l_pcre->init(l_rx.c_str(), l_rx.length());
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        const char* l_err_ptr;
                                        int l_err_off;
                                        l_pcre->get_err_info(&l_err_ptr, l_err_off);
                                        WAFLZ_PERROR(m_err_msg, "Failed to init re from %s. Reason: %s -offset: %d",
                                                     l_rx.c_str(),
                                                     l_err_ptr,
                                                     l_err_off);
                                        if (l_pcre) { delete l_pcre; l_pcre = NULL; }
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_regex_list.push_back(l_pcre);;
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_pcre);
                                break;
                        }
                        // ---------------------------------
                        // VALIDATEBYTERANGE
                        // ---------------------------------
                        case ::waflz_pb::sec_rule_t_operator_t_type_t_VALIDATEBYTERANGE:
                        {
                                int32_t l_s;
                                byte_range* l_br = NULL;
                                l_s = create_byte_range(&l_br, l_rule->operator_().value());
                                if (l_s != WAFLZ_STATUS_OK)
                                {
                                        WAFLZ_PERROR(m_err_msg, "Failed to create byte_range from %s", l_rule->operator_().value().c_str());
                                        return WAFLZ_STATUS_ERROR;
                                }
                                ao_cx_cfg.m_byte_range_list.push_back(l_br);
                                l_rule->mutable_operator_()->set__reserved_1((uint64_t)l_br);
                                break;
                        }
                        default:
                        {
                                break;
                        }
                        }
                        ++l_cr_idx;
                } while(l_cr_idx < l_r.chained_rule_size());
#if 0
                // -----------------------------------------
                // check for missing tx anomaly setting...
                // -----------------------------------------
                if (!l_has_anomaly_score)
                {
                        if (l_is_block &&
                           l_rule->has_action())
                        {
                                NDBG_OUTPUT("%sMISSING ANOMALY%s     RULE: %8s: MSG: %s\n", ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, l_r.action().id().c_str(), l_r.action().msg().c_str());
                                ::waflz_pb::sec_action_t_setvar_t* l_sv = l_rule->mutable_action()->add_setvar();
                                l_sv->set_scope(::waflz_pb::sec_action_t_setvar_t_scope_t_TX);
                                l_sv->set_var("anomaly_score");
                                l_sv->set_op(::waflz_pb::sec_action_t_setvar_t_op_t_INCREMENT);
                                l_sv->set_val("%{tx.critical_anomaly_score}");
                        }
                        else if (l_has_severity)
                        {
                                NDBG_OUTPUT("%sANOMALY_NO_SEVERITY%s RULE: %8s: MSG: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_r.action().id().c_str(), l_r.action().msg().c_str());
                        }
                }
#else
                UNUSED(l_has_anomaly_score);
                UNUSED(l_is_block);
                UNUSED(l_has_severity);
#endif
        }
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t engine::process_include(compiled_config_t** ao_cx_cfg,
                                const std::string& a_include,
                                waflz_pb::sec_config_t& a_config,
                                const std::string& a_ruleset_dir)
{
        // -------------------------------------------------
        // find include in config map...
        // -------------------------------------------------
        compiled_config_map_t::iterator i_cfg;
        i_cfg = m_compiled_config_map.find(a_include);
        if (i_cfg != m_compiled_config_map.end())
        {
                *ao_cx_cfg = i_cfg->second;
                return WAFLZ_STATUS_OK;
        }
        // -------------------------------------------------
        // not found -compile one
        // -------------------------------------------------
        waflz_pb::sec_config_t* l_cfg = new waflz_pb::sec_config_t();
        config_parser* l_parser = new config_parser();
        // -------------------------------------------------
        // default format is modsec
        // -------------------------------------------------
        config_parser::format_t l_format = config_parser::MODSECURITY;
        // -------------------------------------------------
        // Get the file ext to decide format
        // -------------------------------------------------
        if (strncmp(get_file_ext(a_include).c_str(), "json", sizeof("json")) == 0)
        {
                l_format = config_parser::JSON;
        }
        else if (strncmp(get_file_ext(a_include).c_str(), "pbuf", sizeof("pbuf")) == 0)
        {
                l_format = config_parser::PROTOBUF;
        }
        int32_t l_s;
        l_s = l_parser->parse_config(*l_cfg, l_format, a_include);
        if (l_s != WAFLZ_STATUS_OK)
        {
                if (l_parser) { delete l_parser; l_parser = NULL;}
                return WAFLZ_STATUS_ERROR;
        }
        // TODO remove -debug...
        //l_parser->show_status();
        if (l_parser) { delete l_parser; l_parser = NULL;}
        // -------------------------------------------------
        // compile
        // -------------------------------------------------
        compiled_config_t* l_new_cx_cfg = NULL;
        l_new_cx_cfg = new compiled_config_t();
        l_s = compile(*l_new_cx_cfg, *l_cfg, a_ruleset_dir);
        if (l_s != WAFLZ_STATUS_OK)
        {
                if (l_new_cx_cfg) { delete l_new_cx_cfg; l_new_cx_cfg = NULL;}
                return WAFLZ_STATUS_ERROR;
        }
        m_compiled_config_map[a_include] = l_new_cx_cfg;
        m_config_list.push_back(l_cfg);
        *ao_cx_cfg = l_new_cx_cfg;
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t engine::merge(compiled_config_t& ao_cx_cfg,
                      const compiled_config_t& a_cx_cfg)
{
        // -------------------------------------------------
        // phase 1
        // -------------------------------------------------
        const directive_list_t& l_dl_p1 = a_cx_cfg.m_directive_list_phase_1;
        for (directive_list_t::const_iterator i_d = l_dl_p1.begin();
            i_d != l_dl_p1.end();
            ++i_d)
        {
                if (!(*i_d))
                {
                        continue;
                }
                ao_cx_cfg.m_directive_list_phase_1.push_back(*i_d);
                // -----------------------------------------
                // special handling for markers
                // -----------------------------------------
                if ((*i_d)->has_marker())
                {
                        const ::std::string& l_m = (*i_d)->marker();
                        ao_cx_cfg.m_marker_map_phase_1[l_m] = --(ao_cx_cfg.m_directive_list_phase_1.end());
                }
        }
        // -------------------------------------------------
        // phase 2
        // -------------------------------------------------
        const directive_list_t& l_dl_p2 = a_cx_cfg.m_directive_list_phase_2;
        for (directive_list_t::const_iterator i_d = l_dl_p2.begin();
            i_d != l_dl_p2.end();
            ++i_d)
        {
                if (!(*i_d))
                {
                        continue;
                }
                ao_cx_cfg.m_directive_list_phase_2.push_back(*i_d);
                // -----------------------------------------
                // special handling for markers
                // -----------------------------------------
                if ((*i_d)->has_marker())
                {
                        const ::std::string& l_m = (*i_d)->marker();
                        ao_cx_cfg.m_marker_map_phase_2[l_m] = --(ao_cx_cfg.m_directive_list_phase_2.end());
                }
        }
        // -------------------------------------------------
        // phase 3
        // -------------------------------------------------
        const directive_list_t &l_dl_p3 = a_cx_cfg.m_directive_list_phase_3;
        for(directive_list_t::const_iterator i_d = l_dl_p3.begin();
            i_d != l_dl_p3.end();
            ++i_d)
        {
                if(!(*i_d))
                {
                        continue;
                }
                ao_cx_cfg.m_directive_list_phase_3.push_back(*i_d);
                // -----------------------------------------
                // special handling for markers
                // -----------------------------------------
                if((*i_d)->has_marker())
                {
                        const ::std::string& l_m = (*i_d)->marker();
                        ao_cx_cfg.m_marker_map_phase_3[l_m] = --(ao_cx_cfg.m_directive_list_phase_3.end());
                }
        }
        // -------------------------------------------------
        // phase 4
        // -------------------------------------------------
        const directive_list_t &l_dl_p4 = a_cx_cfg.m_directive_list_phase_4;
        for(directive_list_t::const_iterator i_d = l_dl_p4.begin();
            i_d != l_dl_p4.end();
            ++i_d)
        {
                if(!(*i_d))
                {
                        continue;
                }
                ao_cx_cfg.m_directive_list_phase_4.push_back(*i_d);
                // -----------------------------------------
                // special handling for markers
                // -----------------------------------------
                if((*i_d)->has_marker())
                {
                        const ::std::string& l_m = (*i_d)->marker();
                        ao_cx_cfg.m_marker_map_phase_4[l_m] = --(ao_cx_cfg.m_directive_list_phase_4.end());
                }
        }
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
void engine::set_geoip2_dbs(const std::string& a_geoip2_db,
                            const std::string& a_geoip2_isp_db)
{
        m_geoip2_db = a_geoip2_db;
        m_geoip2_isp_db = a_geoip2_isp_db;
}
//! ----------------------------------------------------------------------------
//! \details C binding for third party lib to create an engine obj
//! \return  an engine object
//! \param   void
//! ----------------------------------------------------------------------------
extern "C" engine* create_waflz_engine(void)
{
        return new engine();
}
//! ----------------------------------------------------------------------------
//! \details C binding for third party lib to set the ruleset directory of
//!          waf rulesets for a given scopes config.
//! \return  an engine object
//! \param   a_engine: engine object
//! \param   a_ruleset_dir: location of ruleset directory
//! ----------------------------------------------------------------------------
extern "C" void set_waflz_ruleset_dir(engine* a_engine, char* a_ruleset_dir)
{
        a_engine->set_ruleset_dir(a_ruleset_dir);;
}
//! ----------------------------------------------------------------------------
//! \details C binding for third party lib to set geoip db paths
//! \return  void
//! \param   a_engine: engine obj
//! \param   a_geoip2_db: location of geoip city mmdb file
//! \param   a_geoip2_isp_db: location of geoip isp mmdb file
//! ----------------------------------------------------------------------------
extern "C" void set_waflz_geoip2_dbs(engine* a_engine, char* a_geoip2_db, char* a_geoip2_isp_db)
{
        a_engine->set_geoip2_dbs(a_geoip2_db, a_geoip2_isp_db);;
}
//! ----------------------------------------------------------------------------
//! \details C binding for third party initiate engine obj, this will set
//!          ruleset dir, geoip db, paths, compile waf rules etc
//! \return  0 on success
//!          -1 on failure
//! \param   a_engine: engine object
//! ----------------------------------------------------------------------------
extern "C" int32_t init_waflz_engine(engine* a_engine)
{
        return a_engine->init();;
}
//! ----------------------------------------------------------------------------
//! \details C binding for third party lib to cleanup engine obj
//! \return  0: success
//! \param   a_engine: engine obj
//! ----------------------------------------------------------------------------
extern "C" int32_t waflz_engine_cleanup(engine* a_engine)
{
        if (a_engine)
        {
                delete a_engine;
                a_engine = NULL;
        }
        return WAFLZ_STATUS_OK;
}
}
