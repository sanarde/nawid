/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NAWID_LOOP_H
#define NAWID_LOOP_H

enum nawid_source_kind {
    NAWID_SRC_SIGNAL,
    NAWID_SRC_CONFIG,
    NAWID_SRC_DISPLAY,
    NAWID_SRC_BUS,
};

struct nawid_source {
    enum nawid_source_kind kind;
};

#endif
