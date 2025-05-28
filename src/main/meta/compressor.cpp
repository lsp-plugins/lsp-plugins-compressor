/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-compressor
 * Created on: 3 авг. 2024 г.
 *
 * lsp-plugins-compressor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-compressor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-compressor. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/compressor.h>

#define LSP_PLUGINS_COMPRESSOR_VERSION_MAJOR       1
#define LSP_PLUGINS_COMPRESSOR_VERSION_MINOR       0
#define LSP_PLUGINS_COMPRESSOR_VERSION_MICRO       31

#define LSP_PLUGINS_COMPRESSOR_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_COMPRESSOR_VERSION_MAJOR, \
        LSP_PLUGINS_COMPRESSOR_VERSION_MINOR, \
        LSP_PLUGINS_COMPRESSOR_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Compressor
        static const int plugin_classes[]           = { C_COMPRESSOR, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_STEREO, -1 };

        static const port_item_t comp_sc_modes[] =
        {
            { "Peak",           "sidechain.peak"            },
            { "RMS",            "sidechain.rms"             },
            { "LPF",            "sidechain.lpf"             },
            { "SMA",            "sidechain.sma"             },
            { NULL, NULL }
        };

        static const port_item_t comp_sc_sources[] =
        {
            { "Middle",         "sidechain.middle"          },
            { "Side",           "sidechain.side"            },
            { "Left",           "sidechain.left"            },
            { "Right",          "sidechain.right"           },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t comp_sc_split_sources[] =
        {
            { "Left/Right",     "sidechain.left_right"      },
            { "Right/Left",     "sidechain.right_left"      },
            { "Mid/Side",       "sidechain.mid_side"        },
            { "Side/Mid",       "sidechain.side_mid"        },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t comp_sc_type[] =
        {
            { "Feed-forward",   "sidechain.feed_forward" },
            { "Feed-back",      "sidechain.feed_back" },
            { "Link",           "sidechain.link" },
            { NULL, NULL }
        };

        static const port_item_t comp_sc2_type[] =
        {
            { "Feed-forward",   "sidechain.feed_forward" },
            { "Feed-back",      "sidechain.feed_back" },
            { "External",       "sidechain.external" },
            { "Link",           "sidechain.link" },
            { NULL, NULL }
        };

        static const port_item_t comp_modes[] =
        {
            { "Down",       "compressor.down_ward" },
            { "Up",         "compressor.up_ward" },
            { "Boot",       "compressor.boost_ing" },
            { NULL, NULL }
        };

        static const port_item_t comp_filter_slope[] =
        {
            { "off",        "eq.slope.off"      },
            { "12 dB/oct",  "eq.slope.12dbo"    },
            { "24 dB/oct",  "eq.slope.24dbo"    },
            { "36 dB/oct",  "eq.slope.36dbo"    },
            { NULL, NULL }
        };

        #define COMP_PREMIX \
            SWITCH("showpmx", "Show pre-mix overlay", "Show premix bar", 0.0f), \
            AMP_GAIN10("in2lk", "Input to Link mix", "In to Link mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2in", "Link to Input mix", "Link to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2sc", "Link to Sidechain mix", "Link to SC mix", GAIN_AMP_M_INF_DB)

        #define COMP_SC_PREMIX \
            COMP_PREMIX, \
            AMP_GAIN10("in2sc", "Input to Sidechain mix", "In to SC mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2in", "Sidechain to Input mix", "SC to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2lk", "Sidechain to Link mix", "SC to Link mix", GAIN_AMP_M_INF_DB)

        #define COMP_COMMON     \
            BYPASS,             \
            IN_GAIN,            \
            OUT_GAIN,           \
            SWITCH("showmx", "Show mix overlay", "Show mix bar", 0.0f), \
            SWITCH("showsc", "Show sidechain overlay", "Show SC bar", 0.0f), \
            SWITCH("pause", "Pause graph analysis", "Pause", 0.0f), \
            TRIGGER("clear", "Clear graph analysis", "Clear")

        #define COMP_MS_COMMON  \
            COMP_COMMON,        \
            SWITCH("msl", "Mid/Side listen", "M/S listen", 0.0f)

        #define COMP_SPLIT_COMMON \
            SWITCH("ssplit", "Stereo split", "Stereo split", 0.0f), \
            COMBO("sscs", "Split sidechain source", "Split SC source", compressor_metadata::SC_SPLIT_SOURCE_DFL, comp_sc_split_sources)

        #define COMP_SHM_LINK_MONO \
            OPT_RETURN_MONO("link", "shml", "Side-chain shared memory link")

        #define COMP_SHM_LINK_STEREO \
            OPT_RETURN_STEREO("link", "shml_", "Side-chain shared memory link")

        #define COMP_SC_MONO_CHANNEL(sct, sct_dfl) \
            COMBO("sct", "Sidechain type", "SC type", sct_dfl, sct), \
            COMBO("scm", "Sidechain mode", "SC mode", compressor_metadata::SC_MODE_DFL, comp_sc_modes), \
            CONTROL("sla", "Sidechain lookahead", "SC look", U_MSEC, compressor_metadata::LOOKAHEAD), \
            SWITCH("scl", "Sidechain listen", "SC listen", 0.0f), \
            LOG_CONTROL("scr", "Sidechain reactivity", "SC react", U_MSEC, compressor_metadata::REACTIVITY), \
            AMP_GAIN100("scp", "Sidechain preamp", "SC preamp", GAIN_AMP_0_DB), \
            COMBO("shpm", "High-pass filter mode", "HPF mode", 0, comp_filter_slope),      \
            LOG_CONTROL("shpf", "High-pass filter frequency", "HPF freq", U_HZ, compressor_metadata::HPF),   \
            COMBO("slpm", "Low-pass filter mode", "LPF mode", 0, comp_filter_slope),      \
            LOG_CONTROL("slpf", "Low-pass filter frequency", "LPF freq", U_HZ, compressor_metadata::LPF)

        #define COMP_SC_STEREO_CHANNEL(id, label, alias, sct, sct_dfl) \
            COMBO("sct" id, "Sidechain type" label, "SC type" alias, sct_dfl, sct), \
            COMBO("scm" id, "Sidechain mode" label, "SC mode" alias, compressor_metadata::SC_MODE_DFL, comp_sc_modes), \
            CONTROL("sla" id, "Sidechain lookahead" label, "SC look" alias, U_MSEC, compressor_metadata::LOOKAHEAD), \
            SWITCH("scl" id, "Sidechain listen" label, "SC listen" alias, 0.0f), \
            COMBO("scs" id, "Sidechain source" label, "SC source" alias, compressor_metadata::SC_SOURCE_DFL, comp_sc_sources), \
            LOG_CONTROL("scr" id, "Sidechain reactivity" label, "SC react" alias, U_MSEC, compressor_metadata::REACTIVITY), \
            AMP_GAIN100("scp" id, "Sidechain preamp" label, "SC preamp" alias, GAIN_AMP_0_DB), \
            COMBO("shpm" id, "High-pass filter mode" label, "HPF mode" alias, 0, comp_filter_slope),      \
            LOG_CONTROL("shpf" id, "High-pass filter frequency" label, "HPF freq" alias, U_HZ, compressor_metadata::HPF),   \
            COMBO("slpm" id, "Low-pass filter mode" label, "LPF mode" alias, 0, comp_filter_slope),      \
            LOG_CONTROL("slpf" id, "Low-pass filter frequency" label, "LPF freq" alias, U_HZ, compressor_metadata::LPF)

        #define COMP_CHANNEL(id, label, alias, modes) \
            COMBO("cm" id, "Compression mode" label, "Mode" alias, compressor_metadata::CM_DEFAULT, modes), \
            LOG_CONTROL("al" id, "Attack threshold" label, "Att lvl" alias, U_GAIN_AMP, compressor_metadata::ATTACK_LVL), \
            LOG_CONTROL("at" id, "Attack time" label, "Att time" alias, U_MSEC, compressor_metadata::ATTACK_TIME), \
            LOG_CONTROL("rrl" id, "Release threshold" label, "Rel lvl" alias, U_GAIN_AMP, compressor_metadata::RELEASE_LVL), \
            LOG_CONTROL("rt" id, "Release time" label, "Rel time" alias, U_MSEC, compressor_metadata::RELEASE_TIME), \
            CONTROL("hold" id, "Hold time" label, "Hold time" alias, U_MSEC, compressor_metadata::HOLD_TIME), \
            LOG_CONTROL("cr" id, "Ratio" label, "Ratio" alias, U_NONE, compressor_metadata::RATIO), \
            LOG_CONTROL("kn" id, "Knee" label, "Knee" alias, U_GAIN_AMP, compressor_metadata::KNEE), \
            EXT_LOG_CONTROL("bth" id, "Boost threshold" label, "Boost" alias, U_GAIN_AMP, compressor_metadata::BTH), \
            EXT_LOG_CONTROL("bsa" id, "Boost signal amount" label, "Boost lvl" alias, U_GAIN_AMP, compressor_metadata::BSA), \
            LOG_CONTROL("mk" id, "Makeup gain" label, "Makeup" alias, U_GAIN_AMP, compressor_metadata::MAKEUP), \
            AMP_GAIN10("cdr" id, "Dry gain" label, "Dry" alias, GAIN_AMP_M_INF_DB),     \
            AMP_GAIN10("cwt" id, "Wet gain" label, "Wet" alias, GAIN_AMP_0_DB), \
            PERCENTS("cdw" id, "Dry/Wet balance" label, "Dry/Wet" alias, 100.0f, 0.1f), \
            METER_OUT_GAIN("rl" id, "Release level" label, 20.0f), \
            MESH("ccg" id, "Compressor curve graph" label, 2, compressor_metadata::CURVE_MESH_SIZE)

        #define COMP_AUDIO_METER(id, label, alias) \
            SWITCH("slv" id, "Sidechain level visibility" label, "Show SC" alias, 1.0f), \
            SWITCH("elv" id, "Envelope level visibility" label, "Show Env" alias, 1.0f), \
            SWITCH("grv" id, "Gain reduction visibility" label, "Show Gain" alias, 1.0f), \
            SWITCH("ilv" id, "Input level visibility" label, "Show In" alias, 1.0f), \
            SWITCH("olv" id, "Output level visibility" label, "Show Out" alias, 1.0f), \
            MESH("scg" id, "Compressor sidechain graph" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("evg" id, "Compressor envelope graph" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("grg" id, "Compressor gain reduciton" label, 2, compressor_metadata::TIME_MESH_SIZE + 4), \
            MESH("icg" id, "Compressor input" label, 2, compressor_metadata::TIME_MESH_SIZE + 2), \
            MESH("ocg" id, "Compressor output" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            METER_OUT_GAIN("slm" id, "Sidechain level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("clm" id, "Curve level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("elm" id, "Envelope level meter" label, GAIN_AMP_P_36_DB), \
            METER_GAIN_DFL("rlm" id, "Reduction level meter" label, GAIN_AMP_P_72_DB, GAIN_AMP_0_DB), \
            METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_36_DB), \
            METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_36_DB)

        #define COMP_LINK(id, label, alias) \
            SWITCH(id, label, alias, 0.0f)

        static const port_t compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            COMP_SHM_LINK_MONO,
            COMP_PREMIX,
            COMP_COMMON,
            COMP_SC_MONO_CHANNEL(comp_sc_type, 0),
            COMP_CHANNEL("", "", "", comp_modes),
            COMP_AUDIO_METER("", "", ""),

            PORTS_END
        };

        static const port_t compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_SHM_LINK_STEREO,
            COMP_PREMIX,
            COMP_COMMON,
            COMP_SPLIT_COMMON,
            COMP_SC_STEREO_CHANNEL("", "", "", comp_sc_type, 0),
            COMP_CHANNEL("", "", "", comp_modes),
            COMP_AUDIO_METER("_l", " Left", " L"),
            COMP_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_SHM_LINK_STEREO,
            COMP_PREMIX,
            COMP_COMMON,
            COMP_LINK("clink", "Left/Right controls link", "L/R link"),
            COMP_SC_STEREO_CHANNEL("_l", " Left", " L", comp_sc_type, 0),
            COMP_SC_STEREO_CHANNEL("_r", " Right", " R", comp_sc_type, 0),
            COMP_CHANNEL("_l", " Left", " L", comp_modes),
            COMP_CHANNEL("_r", " Right", " R", comp_modes),
            COMP_AUDIO_METER("_l", " Left", " L"),
            COMP_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_SHM_LINK_STEREO,
            COMP_PREMIX,
            COMP_MS_COMMON,
            COMP_LINK("clink", "Mid/Side controls link", "M/S link"),
            COMP_SC_STEREO_CHANNEL("_m", " Mid", " M", comp_sc_type, 0),
            COMP_SC_STEREO_CHANNEL("_s", " Side", " S", comp_sc_type, 0),
            COMP_CHANNEL("_m", " Mid", " M", comp_modes),
            COMP_CHANNEL("_s", " Side", " S", comp_modes),
            COMP_AUDIO_METER("_m", " Mid", " M"),
            COMP_AUDIO_METER("_s", " Side", " S"),

            PORTS_END
        };

        static const port_t sc_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            COMP_SHM_LINK_MONO,
            COMP_SC_PREMIX,
            COMP_COMMON,
            COMP_SC_MONO_CHANNEL(comp_sc2_type, 2),
            COMP_CHANNEL("", "", "", comp_modes),
            COMP_AUDIO_METER("", "", ""),

            PORTS_END
        };

        static const port_t sc_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_SHM_LINK_STEREO,
            COMP_SC_PREMIX,
            COMP_COMMON,
            COMP_SPLIT_COMMON,
            COMP_SC_STEREO_CHANNEL("", "", "", comp_sc2_type, 2),
            COMP_CHANNEL("", "", "", comp_modes),
            COMP_AUDIO_METER("_l", " Left", " L"),
            COMP_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t sc_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_SHM_LINK_STEREO,
            COMP_SC_PREMIX,
            COMP_COMMON,
            COMP_LINK("clink", "Left/Right controls link", "L/R link"),
            COMP_SC_STEREO_CHANNEL("_l", " Left", " L", comp_sc2_type, 2),
            COMP_SC_STEREO_CHANNEL("_r", " Right", " R", comp_sc2_type, 2),
            COMP_CHANNEL("_l", " Left", " L", comp_modes),
            COMP_CHANNEL("_r", " Right", " R", comp_modes),
            COMP_AUDIO_METER("_l", " Left", " L"),
            COMP_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t sc_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_SHM_LINK_STEREO,
            COMP_SC_PREMIX,
            COMP_MS_COMMON,
            COMP_LINK("clink", "Mid/Side controls link", "M/S link"),
            COMP_SC_STEREO_CHANNEL("_m", " Mid", " M", comp_sc2_type, 2),
            COMP_SC_STEREO_CHANNEL("_s", " Side", " S", comp_sc2_type, 2),
            COMP_CHANNEL("_m", " Mid", " M", comp_modes),
            COMP_CHANNEL("_s", " Side", " S", comp_modes),
            COMP_AUDIO_METER("_m", " Mid", " M"),
            COMP_AUDIO_METER("_s", " Side", " S"),

            PORTS_END
        };

        const meta::bundle_t compressor_bundle =
        {
            "compressor",
            "Compressor",
            B_DYNAMICS,
            "HXOjx3jfw2I",
            "This plugin performs compression of input signal. Flexible sidechain\nconfiguration available. Different types of compression are possible:\ndownward, upward and parallel. Also compressor may work as limiter in\nPeak mode with high Ratio and low Attack time."
        };

        // Compressor
        const meta::plugin_t compressor_mono =
        {
            "Kompressor Mono",
            "Compressor Mono",
            "Compressor Mono",
            "K1M",
            &developers::v_sadovnikov,
            "compressor_mono",
            {
                LSP_LV2_URI("compressor_mono"),
                LSP_LV2UI_URI("compressor_mono"),
                "bgsy",
                LSP_VST3_UID("k1m     bgsy"),
                LSP_VST3UI_UID("k1m     bgsy"),
                LSP_LADSPA_COMPRESSOR_BASE + 0,
                LSP_LADSPA_URI("compressor_mono"),
                LSP_CLAP_URI("compressor_mono"),
                LSP_GST_UID("compressor_mono"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_mono_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/mono",
            mono_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_stereo =
        {
            "Kompressor Stereo",
            "Compressor Stereo",
            "Compressor Stereo",
            "K1S",
            &developers::v_sadovnikov,
            "compressor_stereo",
            {
                LSP_LV2_URI("compressor_stereo"),
                LSP_LV2UI_URI("compressor_stereo"),
                "unsc",
                LSP_VST3_UID("k1s     unsc"),
                LSP_VST3UI_UID("k1s     unsc"),
                LSP_LADSPA_COMPRESSOR_BASE + 1,
                LSP_LADSPA_URI("compressor_stereo"),
                LSP_CLAP_URI("compressor_stereo"),
                LSP_GST_UID("compressor_stereo"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_stereo_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/stereo",
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_lr =
        {
            "Kompressor LeftRight",
            "Compressor LeftRight",
            "Compressor L/R",
            "K1LR",
            &developers::v_sadovnikov,
            "compressor_lr",
            {
                LSP_LV2_URI("compressor_lr"),
                LSP_LV2UI_URI("compressor_lr"),
                "3nam",
                LSP_VST3_UID("k1lr    3nam"),
                LSP_VST3UI_UID("k1lr    3nam"),
                LSP_LADSPA_COMPRESSOR_BASE + 2,
                LSP_LADSPA_URI("compressor_lr"),
                LSP_CLAP_URI("compressor_lr"),
                LSP_GST_UID("compressor_lr"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_lr_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/lr",
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_ms =
        {
            "Kompressor MidSide",
            "Compressor MidSide",
            "Compressor M/S",
            "K1MS",
            &developers::v_sadovnikov,
            "compressor_ms",
            {
                LSP_LV2_URI("compressor_ms"),
                LSP_LV2UI_URI("compressor_ms"),
                "jjef",
                LSP_VST3_UID("k1ms    jjef"),
                LSP_VST3UI_UID("k1ms    jjef"),
                LSP_LADSPA_COMPRESSOR_BASE + 3,
                LSP_LADSPA_URI("compressor_ms"),
                LSP_CLAP_URI("compressor_ms"),
                LSP_GST_UID("compressor_ms"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_ms_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/ms",
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        // Sidechain compressor
        const meta::plugin_t  sc_compressor_mono =
        {
            "Sidechain-Kompressor Mono",
            "Sidechain Compressor Mono",
            "SC Compressor Mono",
            "SCK1M",
            &developers::v_sadovnikov,
            "sc_compressor_mono",
            {
                LSP_LV2_URI("sc_compressor_mono"),
                LSP_LV2UI_URI("sc_compressor_mono"),
                "lyjq",
                LSP_VST3_UID("sck1m   lyjq"),
                LSP_VST3UI_UID("sck1m   lyjq"),
                LSP_LADSPA_COMPRESSOR_BASE + 4,
                LSP_LADSPA_URI("sc_compressor_mono"),
                LSP_CLAP_URI("sc_compressor_mono"),
                LSP_GST_UID("sc_compressor_mono"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_mono_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/mono",
            mono_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_stereo =
        {
            "Sidechain-Kompressor Stereo",
            "Sidechain Compressor Stereo",
            "SC Compressor Stereo",
            "SCK1S",
            &developers::v_sadovnikov,
            "sc_compressor_stereo",
            {
                LSP_LV2_URI("sc_compressor_stereo"),
                LSP_LV2UI_URI("sc_compressor_stereo"),
                "5xzi",
                LSP_VST3_UID("sck1s   5xzi"),
                LSP_VST3UI_UID("sck1s   5xzi"),
                LSP_LADSPA_COMPRESSOR_BASE + 5,
                LSP_LADSPA_URI("sc_compressor_stereo"),
                LSP_CLAP_URI("sc_compressor_stereo"),
                LSP_GST_UID("sc_compressor_stereo"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_stereo_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/stereo",
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_lr =
        {
            "Sidechain-Kompressor LeftRight",
            "Sidechain Compressor LeftRight",
            "SC Compressor L/R",
            "SCK1LR",
            &developers::v_sadovnikov,
            "sc_compressor_lr",
            {
                LSP_LV2_URI("sc_compressor_lr"),
                LSP_LV2UI_URI("sc_compressor_lr"),
                "fowg",
                LSP_VST3_UID("sck1lr  fowg"),
                LSP_VST3UI_UID("sck1lr  fowg"),
                LSP_LADSPA_COMPRESSOR_BASE + 6,
                LSP_LADSPA_URI("sc_compressor_lr"),
                LSP_CLAP_URI("sc_compressor_lr"),
                LSP_GST_UID("sc_compressor_lr"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_lr_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/lr",
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_ms =
        {
            "Sidechain-Kompressor MidSide",
            "Sidechain Compressor MidSide",
            "SC Compressor M/S",
            "SCK1MS",
            &developers::v_sadovnikov,
            "sc_compressor_ms",
            {
                LSP_LV2_URI("sc_compressor_ms"),
                LSP_LV2UI_URI("sc_compressor_ms"),
                "ioqg",
                LSP_VST3_UID("sck1ms  ioqg"),
                LSP_VST3UI_UID("sck1ms  ioqg"),
                LSP_LADSPA_COMPRESSOR_BASE + 7,
                LSP_LADSPA_URI("sc_compressor_ms"),
                LSP_CLAP_URI("sc_compressor_ms"),
                LSP_GST_UID("sc_compressor_ms"),
            },
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_ms_ports,
            "dynamics/compressor/single.xml",
            "dynamics/compressor/single/ms",
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };
    } /* namespace meta */
} /* namespace lsp */
