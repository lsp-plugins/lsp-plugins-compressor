/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-compressor
 * Created on: 3 авг. 2021 г.
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
#define LSP_PLUGINS_COMPRESSOR_VERSION_MICRO       21

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
            { NULL, NULL }
        };

        static const port_item_t comp_sc2_type[] =
        {
            { "Feed-forward",   "sidechain.feed_forward" },
            { "Feed-back",      "sidechain.feed_back" },
            { "External",       "sidechain.external" },
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

        #define COMP_COMMON     \
            BYPASS,             \
            IN_GAIN,            \
            OUT_GAIN,           \
            SWITCH("pause", "Pause graph analysis", 0.0f), \
            TRIGGER("clear", "Clear graph analysis")

        #define COMP_MS_COMMON  \
            COMP_COMMON,        \
            SWITCH("msl", "Mid/Side listen", 0.0f)

        #define COMP_SPLIT_COMMON \
            SWITCH("ssplit", "Stereo split", 0.0f), \
            COMBO("sscs", "Split sidechain source", compressor_metadata::SC_SPLIT_SOURCE_DFL, comp_sc_split_sources)

        #define COMP_SC_MONO_CHANNEL(sct) \
            COMBO("sct", "Sidechain type", compressor_metadata::SC_TYPE_DFL, sct), \
            COMBO("scm", "Sidechain mode", compressor_metadata::SC_MODE_DFL, comp_sc_modes), \
            CONTROL("sla", "Sidechain lookahead", U_MSEC, compressor_metadata::LOOKAHEAD), \
            SWITCH("scl", "Sidechain listen", 0.0f), \
            LOG_CONTROL("scr", "Sidechain reactivity", U_MSEC, compressor_metadata::REACTIVITY), \
            AMP_GAIN100("scp", "Sidechain preamp", GAIN_AMP_0_DB), \
            COMBO("shpm", "High-pass filter mode", 0, comp_filter_slope),      \
            LOG_CONTROL("shpf", "High-pass filter frequency", U_HZ, compressor_metadata::HPF),   \
            COMBO("slpm", "Low-pass filter mode", 0, comp_filter_slope),      \
            LOG_CONTROL("slpf", "Low-pass filter frequency", U_HZ, compressor_metadata::LPF)

        #define COMP_SC_STEREO_CHANNEL(id, label, sct) \
            COMBO("sct" id, "Sidechain type" label, compressor_metadata::SC_TYPE_DFL, sct), \
            COMBO("scm" id, "Sidechain mode" label, compressor_metadata::SC_MODE_DFL, comp_sc_modes), \
            CONTROL("sla" id, "Sidechain lookahead" label, U_MSEC, compressor_metadata::LOOKAHEAD), \
            SWITCH("scl" id, "Sidechain listen" label, 0.0f), \
            COMBO("scs" id, "Sidechain source" label, compressor_metadata::SC_SOURCE_DFL, comp_sc_sources), \
            LOG_CONTROL("scr" id, "Sidechain reactivity" label, U_MSEC, compressor_metadata::REACTIVITY), \
            AMP_GAIN100("scp" id, "Sidechain preamp" label, GAIN_AMP_0_DB), \
            COMBO("shpm" id, "High-pass filter mode" label, 0, comp_filter_slope),      \
            LOG_CONTROL("shpf" id, "High-pass filter frequency" label, U_HZ, compressor_metadata::HPF),   \
            COMBO("slpm" id, "Low-pass filter mode" label, 0, comp_filter_slope),      \
            LOG_CONTROL("slpf" id, "Low-pass filter frequency" label, U_HZ, compressor_metadata::LPF)

        #define COMP_CHANNEL(id, label, modes) \
            COMBO("cm" id, "Compression mode" label, compressor_metadata::CM_DEFAULT, modes), \
            LOG_CONTROL("al" id, "Attack threshold" label, U_GAIN_AMP, compressor_metadata::ATTACK_LVL), \
            LOG_CONTROL("at" id, "Attack time" label, U_MSEC, compressor_metadata::ATTACK_TIME), \
            LOG_CONTROL("rrl" id, "Release threshold" label, U_GAIN_AMP, compressor_metadata::RELEASE_LVL), \
            LOG_CONTROL("rt" id, "Release time" label, U_MSEC, compressor_metadata::RELEASE_TIME), \
            LOG_CONTROL("cr" id, "Ratio" label, U_NONE, compressor_metadata::RATIO), \
            LOG_CONTROL("kn" id, "Knee" label, U_GAIN_AMP, compressor_metadata::KNEE), \
            EXT_LOG_CONTROL("bth" id, "Boost threshold" label, U_GAIN_AMP, compressor_metadata::BTH), \
            EXT_LOG_CONTROL("bsa" id, "Boost signal amount" label, U_GAIN_AMP, compressor_metadata::BSA), \
            LOG_CONTROL("mk" id, "Makeup gain" label, U_GAIN_AMP, compressor_metadata::MAKEUP), \
            AMP_GAIN10("cdr" id, "Dry gain" label, GAIN_AMP_M_INF_DB),     \
            AMP_GAIN10("cwt" id, "Wet gain" label, GAIN_AMP_0_DB), \
            METER_OUT_GAIN("rl" id, "Release level" label, 20.0f), \
            MESH("ccg" id, "Compressor curve graph" label, 2, compressor_metadata::CURVE_MESH_SIZE)

        #define COMP_AUDIO_METER(id, label) \
            SWITCH("slv" id, "Sidechain level visibility" label, 1.0f), \
            SWITCH("elv" id, "Envelope level visibility" label, 1.0f), \
            SWITCH("grv" id, "Gain reduction visibility" label, 1.0f), \
            SWITCH("ilv" id, "Input level visibility" label, 1.0f), \
            SWITCH("olv" id, "Output level visibility" label, 1.0f), \
            MESH("scg" id, "Compressor sidechain graph" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("evg" id, "Compressor envelope graph" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("grg" id, "Compressor gain reduciton" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("icg" id, "Compressor input" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            MESH("ocg" id, "Compressor output" label, 2, compressor_metadata::TIME_MESH_SIZE), \
            METER_OUT_GAIN("slm" id, "Sidechain level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("clm" id, "Curve level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("elm" id, "Envelope level meter" label, GAIN_AMP_P_36_DB), \
            METER_GAIN_DFL("rlm" id, "Reduction level meter" label, GAIN_AMP_P_72_DB, GAIN_AMP_0_DB), \
            METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_36_DB), \
            METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_36_DB)

        static const port_t compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            COMP_COMMON,
            COMP_SC_MONO_CHANNEL(comp_sc_type),
            COMP_CHANNEL("", "", comp_modes),
            COMP_AUDIO_METER("", ""),

            PORTS_END
        };

        static const port_t compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_COMMON,
            COMP_SPLIT_COMMON,
            COMP_SC_STEREO_CHANNEL("", "", comp_sc_type),
            COMP_CHANNEL("", "", comp_modes),
            COMP_AUDIO_METER("_l", " Left"),
            COMP_AUDIO_METER("_r", " Right"),

            PORTS_END
        };

        static const port_t compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_COMMON,
            COMP_SC_STEREO_CHANNEL("_l", " Left", comp_sc_type),
            COMP_SC_STEREO_CHANNEL("_r", " Right", comp_sc_type),
            COMP_CHANNEL("_l", " Left", comp_modes),
            COMP_CHANNEL("_r", " Right", comp_modes),
            COMP_AUDIO_METER("_l", " Left"),
            COMP_AUDIO_METER("_r", " Right"),

            PORTS_END
        };

        static const port_t compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            COMP_MS_COMMON,
            COMP_SC_STEREO_CHANNEL("_m", " Mid", comp_sc_type),
            COMP_SC_STEREO_CHANNEL("_s", " Side", comp_sc_type),
            COMP_CHANNEL("_m", " Mid", comp_modes),
            COMP_CHANNEL("_s", " Side", comp_modes),
            COMP_AUDIO_METER("_m", " Mid"),
            COMP_AUDIO_METER("_s", " Side"),

            PORTS_END
        };

        static const port_t sc_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            COMP_COMMON,
            COMP_SC_MONO_CHANNEL(comp_sc2_type),
            COMP_CHANNEL("", "", comp_modes),
            COMP_AUDIO_METER("", ""),

            PORTS_END
        };

        static const port_t sc_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_COMMON,
            COMP_SPLIT_COMMON,
            COMP_SC_STEREO_CHANNEL("", "", comp_sc2_type),
            COMP_CHANNEL("", "", comp_modes),
            COMP_AUDIO_METER("_l", " Left"),
            COMP_AUDIO_METER("_r", " Right"),

            PORTS_END
        };

        static const port_t sc_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_COMMON,
            COMP_SC_STEREO_CHANNEL("_l", " Left", comp_sc2_type),
            COMP_SC_STEREO_CHANNEL("_r", " Right", comp_sc2_type),
            COMP_CHANNEL("_l", " Left", comp_modes),
            COMP_CHANNEL("_r", " Right", comp_modes),
            COMP_AUDIO_METER("_l", " Left"),
            COMP_AUDIO_METER("_r", " Right"),

            PORTS_END
        };

        static const port_t sc_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            COMP_MS_COMMON,
            COMP_SC_STEREO_CHANNEL("_m", " Mid", comp_sc2_type),
            COMP_SC_STEREO_CHANNEL("_s", " Side", comp_sc2_type),
            COMP_CHANNEL("_m", " Mid", comp_modes),
            COMP_CHANNEL("_s", " Side", comp_modes),
            COMP_AUDIO_METER("_m", " Mid"),
            COMP_AUDIO_METER("_s", " Side"),

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
            "K1M",
            &developers::v_sadovnikov,
            "compressor_mono",
            LSP_LV2_URI("compressor_mono"),
            LSP_LV2UI_URI("compressor_mono"),
            "bgsy",
            LSP_LADSPA_COMPRESSOR_BASE + 0,
            LSP_LADSPA_URI("compressor_mono"),
            LSP_CLAP_URI("compressor_mono"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_mono_ports,
            "dynamics/compressor/single/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_stereo =
        {
            "Kompressor Stereo",
            "Compressor Stereo",
            "K1S",
            &developers::v_sadovnikov,
            "compressor_stereo",
            LSP_LV2_URI("compressor_stereo"),
            LSP_LV2UI_URI("compressor_stereo"),
            "unsc",
            LSP_LADSPA_COMPRESSOR_BASE + 1,
            LSP_LADSPA_URI("compressor_stereo"),
            LSP_CLAP_URI("compressor_stereo"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_stereo_ports,
            "dynamics/compressor/single/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_lr =
        {
            "Kompressor LeftRight",
            "Compressor LeftRight",
            "K1LR",
            &developers::v_sadovnikov,
            "compressor_lr",
            LSP_LV2_URI("compressor_lr"),
            LSP_LV2UI_URI("compressor_lr"),
            "3nam",
            LSP_LADSPA_COMPRESSOR_BASE + 2,
            LSP_LADSPA_URI("compressor_lr"),
            LSP_CLAP_URI("compressor_lr"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_lr_ports,
            "dynamics/compressor/single/lr.xml",
            NULL,
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  compressor_ms =
        {
            "Kompressor MidSide",
            "Compressor MidSide",
            "K1MS",
            &developers::v_sadovnikov,
            "compressor_ms",
            LSP_LV2_URI("compressor_ms"),
            LSP_LV2UI_URI("compressor_ms"),
            "jjef",
            LSP_LADSPA_COMPRESSOR_BASE + 3,
            LSP_LADSPA_URI("compressor_ms"),
            LSP_CLAP_URI("compressor_ms"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            compressor_ms_ports,
            "dynamics/compressor/single/ms.xml",
            NULL,
            stereo_plugin_port_groups,
            &compressor_bundle
        };

        // Sidechain compressor
        const meta::plugin_t  sc_compressor_mono =
        {
            "Sidechain-Kompressor Mono",
            "Sidechain Compressor Mono",
            "SCK1M",
            &developers::v_sadovnikov,
            "sc_compressor_mono",
            LSP_LV2_URI("sc_compressor_mono"),
            LSP_LV2UI_URI("sc_compressor_mono"),
            "lyjq",
            LSP_LADSPA_COMPRESSOR_BASE + 4,
            LSP_LADSPA_URI("sc_compressor_mono"),
            LSP_CLAP_URI("sc_compressor_mono"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_mono_ports,
            "dynamics/compressor/single/mono.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_stereo =
        {
            "Sidechain-Kompressor Stereo",
            "Sidechain Compressor Stereo",
            "SCK1S",
            &developers::v_sadovnikov,
            "sc_compressor_stereo",
            LSP_LV2_URI("sc_compressor_stereo"),
            LSP_LV2UI_URI("sc_compressor_stereo"),
            "5xzi",
            LSP_LADSPA_COMPRESSOR_BASE + 5,
            LSP_LADSPA_URI("sc_compressor_stereo"),
            LSP_CLAP_URI("sc_compressor_stereo"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_stereo_ports,
            "dynamics/compressor/single/stereo.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_lr =
        {
            "Sidechain-Kompressor LeftRight",
            "Sidechain Compressor LeftRight",
            "SCK1LR",
            &developers::v_sadovnikov,
            "sc_compressor_lr",
            LSP_LV2_URI("sc_compressor_lr"),
            LSP_LV2UI_URI("sc_compressor_lr"),
            "fowg",
            LSP_LADSPA_COMPRESSOR_BASE + 6,
            LSP_LADSPA_URI("sc_compressor_lr"),
            LSP_CLAP_URI("sc_compressor_lr"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_lr_ports,
            "dynamics/compressor/single/lr.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };

        const meta::plugin_t  sc_compressor_ms =
        {
            "Sidechain-Kompressor MidSide",
            "Sidechain Compressor MidSide",
            "SCK1MS",
            &developers::v_sadovnikov,
            "sc_compressor_ms",
            LSP_LV2_URI("sc_compressor_ms"),
            LSP_LV2UI_URI("sc_compressor_ms"),
            "ioqg",
            LSP_LADSPA_COMPRESSOR_BASE + 7,
            LSP_LADSPA_URI("sc_compressor_ms"),
            LSP_CLAP_URI("sc_compressor_ms"),
            LSP_PLUGINS_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            sc_compressor_ms_ports,
            "dynamics/compressor/single/ms.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &compressor_bundle
        };
    } /* namespace meta */
} /* namespace lsp */
