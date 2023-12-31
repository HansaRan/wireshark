/* packet-mausb.h
 * Definitions for Media Agnostic USB dissection
 * Copyright 2016, Intel Corporation
 * Author: Sean O. Stalley <sean.stalley@intel.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PACKET_MAUSB_H__
#define __PACKET_MAUSB_H__

#define MAUSB_DPH_LENGTH 20
/** Common header fields, per section 6.2.1 */
struct mausb_header {
    /* DWORD 0 */
    guint8   ver_flags;
    guint8   type;
    guint16  length;
    /* DWORD 1 */
    guint16  handle;
    guint8   ma_dev_addr;
    guint8   mass_id;
    /* DWORD 2 */
    guint8   status;
    union {
        guint16 token;
        struct {
            guint8   eps_tflags;
            union {
                guint16  stream_id;
                guint16  num_headers_iflags;
            } u1;
            /* DWORD 3 */
            guint32  seq_num; /* Note: only 24 bits used */
            guint8   req_id;
            /* DWORD 4 */
            union {
                guint32  credit;
                guint32  present_time_num_seg;
            } u2;
            /* DWORD 5 */
            guint32  timestamp;
            /* DWORD 6 */
            guint32  tx_dly; /* Note: if no timestamp, tx_dly will be in DWORD 5 */
        } s;
    } u;
};

gboolean mausb_is_from_host(struct mausb_header *header);
guint8 mausb_ep_handle_ep_d(guint16 handle);
guint8 mausb_ep_handle_ep_num(guint16 handle);
guint8 mausb_ep_handle_dev_addr(guint16 handle);
guint8 mausb_ep_handle_bus_num(guint16 handle);

void mausb_set_usb_conv_info(usb_conv_info_t *usb_conv_info,
                             struct mausb_header *header);

#endif

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
