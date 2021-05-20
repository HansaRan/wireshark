/* packet-wow.c
 * Routines for World of Warcraft (WoW) protocol dissection
 * Copyright 2008-2009, Stephen Fisher (see AUTHORS file)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* This dissector is based on the MaNGOS project's source code, Stanford's
 * SRP protocol documents (http://srp.stanford.edu) and RFC 2945: "The SRP
 * Authentication and Key Exchange System." */

#include "config.h"

#include <epan/packet.h>
#include <epan/prefs.h>
#include "packet-tcp.h"

void proto_register_wow(void);
void proto_reg_handoff_wow(void);
static void parse_logon_reconnect_proof(tvbuff_t *tvb, packet_info *pinfo, proto_tree *wow_tree, guint32 offset);
static void parse_logon_reconnect_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset);
static void parse_logon_challenge_client_to_server(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset);
static void parse_logon_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset);
static void parse_logon_proof(tvbuff_t *tvb, packet_info* pinfo, proto_tree *wow_tree, guint32 offset);
static void parse_realm_list_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree,
					      guint32 offset, proto_tree *wow_realms_tree);

typedef enum {
	AUTH_LOGON_CHALLENGE       = 0x00,
	AUTH_LOGON_PROOF           = 0x01,
	AUTH_LOGON_RECONNECT       = 0x02,
	AUTH_LOGON_RECONNECT_PROOF = 0x03,
	REALM_LIST                 = 0x10,
	XFER_INITIATE              = 0x30,
	XFER_DATA                  = 0x31,
	XFER_ACCEPT                = 0x32,
	XFER_RESUME                = 0x33,
	XFER_CANCEL                = 0x34
} auth_cmd_e;

static const value_string cmd_vs[] = {
	{ AUTH_LOGON_CHALLENGE,       "Authentication Logon Challenge"     },
	{ AUTH_LOGON_PROOF,           "Authentication Logon Proof"         },
	{ AUTH_LOGON_RECONNECT,       "Authentication Reconnect Challenge" },
	{ AUTH_LOGON_RECONNECT_PROOF, "Authentication Reconnect Proof"     },
	{ REALM_LIST,                 "Realm List"                         },
	{ XFER_INITIATE,              "Transfer Initiate"                  },
	{ XFER_DATA,                  "Transfer Data"                      },
	{ XFER_ACCEPT,                "Transfer Accept"                    },
	{ XFER_RESUME,                "Transfer Resume"                    },
	{ XFER_CANCEL,                "Transfer Cancel"                    },
	{ 0, NULL                                                          }
};

typedef enum {
	SUCCESS                 = 0x00,
	FAIL_UNKNOWN0           = 0x01,
	FAIL_UNKNOWN1           = 0x02,
	FAIL_BANNED             = 0x03,
	FAIL_UNKNOWN_ACCOUNT    = 0x04,
	FAIL_INCORRECT_PASSWORD = 0x05,
	FAIL_ALREADY_ONLINE     = 0x06,
	FAIL_NO_TIME            = 0x07,
	FAIL_DB_BUSY            = 0x08,
	FAIL_VERSION_INVALID    = 0x09,
	FAIL_VERSION_UPDATE     = 0x0A,
	FAIL_INVALID_SERVER     = 0x0B,
	FAIL_SUSPENDED          = 0x0C,
	FAIL_NOACCESS           = 0x0D,
	SUCCESS_SURVEY          = 0x0E,
	FAIL_PARENTAL_CONTROL   = 0x0F
} auth_error_e;

static const value_string error_vs[] = {
	{ SUCCESS,                 "Success" },
	{ FAIL_UNKNOWN0,           "Unknown" },
	{ FAIL_UNKNOWN1,           "Unknown" },
	{ FAIL_BANNED,             "Account banned" },
	{ FAIL_UNKNOWN_ACCOUNT,    "Unknown account" },
	{ FAIL_INCORRECT_PASSWORD, "Incorrect password" },
	{ FAIL_ALREADY_ONLINE,     "Already online" },
	{ FAIL_NO_TIME,            "No game time on account" },
	{ FAIL_DB_BUSY,            "Database busy (could not log in)" },
	{ FAIL_VERSION_INVALID,    "Invalid game version" },
	{ FAIL_VERSION_UPDATE,     "Failed version update" },
	{ FAIL_INVALID_SERVER,     "Invalid server" },
	{ FAIL_SUSPENDED,          "Account suspended" },
	{ FAIL_NOACCESS,           "Unable to connect" },
	{ SUCCESS_SURVEY,          "Survey success" },
	{ FAIL_PARENTAL_CONTROL,   "Blocked by parental controls" },
	{ 0, NULL }
};

static const value_string realm_flags_vs[] = {
	{ 0, "Online"  },
	{ 1, "Locked"  },
	{ 2, "Offline" },
	{ 0, NULL      }
};

static const value_string realm_type_vs[] = {
	{ 0, "Normal"                             },
	{ 1, "Player versus player"               },
	{ 4, "Normal (2)"                         },
	{ 6, "Role playing normal"                },
	{ 8, "Role playing player versus player)" },
	{ 0, NULL                                 }
};

#define WOW_PORT 3724

#define WOW_CLIENT_TO_SERVER pinfo->destport == WOW_PORT
#define WOW_SERVER_TO_CLIENT pinfo->srcport  == WOW_PORT

/* Initialize the protocol and registered fields */
static int proto_wow = -1;

static int hf_wow_command = -1;
static int hf_wow_error = -1;
static int hf_wow_protocol_version = -1;
static int hf_wow_pkt_size = -1;

static struct {
    int gamename;
    int platform;
    int os;
    int country;
    int timezone_bias;
    int ip;
    int srp_i_len;
    int srp_i;
} hf_wow_logon_challenge_client_to_server = {-1, -1, -1, -1, -1, -1, -1, -1};

static struct {
    int major;
    int minor;
    int patch;
    int revision;
} hf_wow_version = {-1, -1, -1, -1};

static struct {
    int srp_b;
    int srp_g_len;
    int srp_g;
    int srp_n_len;
    int srp_n;
    int srp_s;
    int crc_salt;
    int pin_grid_seed;
} hf_wow_logon_challenge_server_to_client = {-1, -1, -1, -1, -1, -1, -1, -1};

static struct {
    int srp_a;
    int srp_m1;
    int crc_hash;
    int pin_hash;
} hf_wow_logon_proof_client_to_server = {-1, -1, -1, -1};

static struct {
    int srp_m2;
    int hardware_survey_id;
} hf_wow_logon_proof_server_to_client = {-1, -1};

static struct {
    int checksum_salt;
} hf_wow_reconnect_challenge_server_to_client = {-1};

static int hf_wow_two_factor_enabled = -1;

static int hf_wow_num_keys = -1;

static int hf_wow_challenge_data = -1;

static int hf_wow_client_proof = -1;
static int hf_wow_client_checksum = -1;

static int hf_wow_two_factor_pin_salt = -1;

static int hf_wow_num_realms = -1;
static int hf_wow_realm_type = -1;
static int hf_wow_realm_flags = -1;
static int hf_wow_realm_category = -1;
static int hf_wow_realm_name = -1;
static int hf_wow_realm_socket = -1;
static int hf_wow_realm_population_level = -1;
static int hf_wow_realm_num_characters = -1;
static int hf_wow_realm_id = -1;

static gboolean wow_preference_desegment = TRUE;

static gint ett_wow = -1;
static gint ett_wow_realms = -1;

struct game_version {
	gint8 major_version;
	gint8 minor_version;
	gint8 patch_version;
	gint16 revision;
};
static struct game_version client_game_version = { -1, -1, -1, -1 };

// WoW uses a kind of SemVer.
// So 1.0.0 is always greater than any 0.x.y, and
// 1.2.0 is always greater than any 1.1.y
static gboolean
version_is_at_or_above(int major, int minor, int patch)
{
	if (client_game_version.major_version > major) {
		return TRUE;
	}
	else if (client_game_version.major_version < major) {
		return FALSE;
	}
	// Major versions must be equal

	if (client_game_version.minor_version > minor) {
		return TRUE;
	}
	else if (client_game_version.minor_version < minor) {
		return FALSE;
	}
	// Both major and minor versions are equal

	if (client_game_version.patch_version > patch) {
		return TRUE;
	}
	else if (client_game_version.patch_version < patch) {
		return FALSE;
	}
	// All versions are identical

	return TRUE;
}

static guint
get_wow_pdu_len(packet_info *pinfo, tvbuff_t *tvb, int offset, void *data _U_)
{
	gint8 size_field_offset = -1;
	guint8 cmd;
	guint16 pkt_len;

	cmd = tvb_get_guint8(tvb, offset);

	if(WOW_SERVER_TO_CLIENT && cmd == REALM_LIST)
		size_field_offset = 1;
	if(WOW_CLIENT_TO_SERVER && cmd == AUTH_LOGON_CHALLENGE)
		size_field_offset = 2;

	pkt_len = tvb_get_letohs(tvb, size_field_offset);

	return pkt_len + size_field_offset + 2;
}

static int
dissect_wow_pdu(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	proto_item *ti;
	proto_tree *wow_tree, *wow_realms_tree;

	guint8 cmd;
	guint32 offset = 0;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, "WOW");

	col_clear(pinfo->cinfo, COL_INFO);

	cmd = tvb_get_guint8(tvb, offset);

	col_set_str(pinfo->cinfo, COL_INFO,
			    val_to_str_const(cmd, cmd_vs,
				       "Unrecognized packet type"));

	if(!tree) {
		return tvb_captured_length(tvb);
	}

	ti = proto_tree_add_item(tree, proto_wow, tvb, 0, -1, ENC_NA);
	wow_tree = proto_item_add_subtree(ti, ett_wow);

	proto_tree_add_item(wow_tree, hf_wow_command, tvb, offset, 1,
			ENC_LITTLE_ENDIAN);
	offset += 1;

	switch(cmd) {

		case AUTH_LOGON_RECONNECT_PROOF:
			parse_logon_reconnect_proof(tvb, pinfo, wow_tree, offset);

			break;

		case AUTH_LOGON_RECONNECT:
			if (WOW_SERVER_TO_CLIENT) {
				parse_logon_reconnect_challenge_server_to_client(tvb, wow_tree, offset);
			} else if (WOW_CLIENT_TO_SERVER) {
				parse_logon_challenge_client_to_server(tvb, wow_tree, offset);
			}

			break;

		case AUTH_LOGON_CHALLENGE :
			if(WOW_CLIENT_TO_SERVER) {
				parse_logon_challenge_client_to_server(tvb, wow_tree, offset);
			} else if(WOW_SERVER_TO_CLIENT) {
				parse_logon_challenge_server_to_client(tvb, wow_tree, offset);
			}

			break;

		case AUTH_LOGON_PROOF :
			parse_logon_proof(tvb, pinfo, wow_tree, offset);

			break;

		case REALM_LIST :
			if(WOW_CLIENT_TO_SERVER) {

			} else if(WOW_SERVER_TO_CLIENT) {
				parse_realm_list_server_to_client(tvb, wow_tree, offset, wow_realms_tree);

			}

			break;
	}

	return tvb_captured_length(tvb);
}

static void
parse_logon_reconnect_proof(tvbuff_t *tvb, packet_info *pinfo, proto_tree *wow_tree, guint32 offset) {
	if (WOW_CLIENT_TO_SERVER) {
		proto_tree_add_item(wow_tree, hf_wow_challenge_data, tvb,
				offset, 16, ENC_NA);
		offset += 16;

		proto_tree_add_item(wow_tree, hf_wow_client_proof, tvb,
				offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_client_checksum, tvb,
				offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_num_keys,
				tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

	}
	else if (WOW_SERVER_TO_CLIENT) {
		proto_tree_add_item(wow_tree, hf_wow_error, tvb,
				offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

	}

}

static void
parse_logon_reconnect_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset) {
	guint8 error = tvb_get_guint8(tvb, offset);

	proto_tree_add_item(wow_tree, hf_wow_error, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;
	if (error != SUCCESS) {
		// Following fields are only present when not an error.
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_challenge_data, tvb,
			offset, 16, ENC_NA);
	offset += 16;

	proto_tree_add_item(wow_tree, hf_wow_reconnect_challenge_server_to_client.checksum_salt, tvb,
			    offset, 16, ENC_NA);
	offset += 16;
}

static void
parse_logon_challenge_client_to_server(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset) {
	guint8 srp_i_len;
	gchar *string;

	proto_tree_add_item(wow_tree, hf_wow_protocol_version, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_pkt_size,
			tvb, offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

	string = g_strreverse(tvb_get_string_enc(wmem_packet_scope(), tvb, offset, 4, ENC_ASCII));
	proto_tree_add_string(wow_tree, hf_wow_logon_challenge_client_to_server.gamename,
			      tvb, offset, 4, string);
	offset += 4;



	client_game_version.major_version = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_version.major,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	client_game_version.minor_version = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_version.minor,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	client_game_version.patch_version = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_version.patch,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	client_game_version.revision = tvb_get_guint16(tvb, offset, ENC_LITTLE_ENDIAN);
	proto_tree_add_item(wow_tree, hf_wow_version.revision, tvb,
			offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

	string = g_strreverse(tvb_get_string_enc(wmem_packet_scope(), tvb, offset, 4, ENC_ASCII));
	proto_tree_add_string(wow_tree, hf_wow_logon_challenge_client_to_server.platform,
			      tvb, offset, 4, string);
	offset += 4;

	string = g_strreverse(tvb_get_string_enc(wmem_packet_scope(), tvb, offset, 4, ENC_ASCII));
	proto_tree_add_string(wow_tree, hf_wow_logon_challenge_client_to_server.os, tvb,
			      offset, 4, string);
	offset += 4;

	string = g_strreverse(tvb_get_string_enc(wmem_packet_scope(), tvb, offset, 4, ENC_ASCII));
	proto_tree_add_string(wow_tree, hf_wow_logon_challenge_client_to_server.country,
			      tvb, offset, 4, string);
	offset += 4;

	proto_tree_add_item(wow_tree,
			    hf_wow_logon_challenge_client_to_server.timezone_bias,
			    tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_client_to_server.ip, tvb,
			    offset, 4, ENC_BIG_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree,
			    hf_wow_logon_challenge_client_to_server.srp_i_len,
			    tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_i_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree,
			    hf_wow_logon_challenge_client_to_server.srp_i, tvb,
			    offset, srp_i_len,
			ENC_ASCII|ENC_NA);
	/*offset += srp_i_len;*/
}

static void
parse_logon_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset) {
    guint8 error, srp_g_len, srp_n_len, two_factor_enabled;

	proto_tree_add_item(wow_tree, hf_wow_protocol_version, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	error = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_error, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;
	if (error != SUCCESS) {
		// Following fields are only present when not an error.
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_b, tvb,
			offset, 32, ENC_NA);
	offset += 32;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_g_len,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_g_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_g, tvb,
			offset, srp_g_len, ENC_NA);
	offset += srp_g_len;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_n_len,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_n_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_n, tvb,
			offset, srp_n_len, ENC_NA);
	offset += srp_n_len;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.srp_s, tvb,
			offset, 32, ENC_NA);
	offset += 32;

	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.crc_salt, tvb,
			offset, 16, ENC_NA);
	offset += 16;

	if (!version_is_at_or_above(1, 12, 0)) {
		/* The two factor fields were added in the 1.12 update. */
		return;
	}
	two_factor_enabled = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_two_factor_enabled, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	if (!two_factor_enabled) {
		return;
	}
	proto_tree_add_item(wow_tree, hf_wow_logon_challenge_server_to_client.pin_grid_seed, tvb,
			offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_two_factor_pin_salt, tvb,
			offset, 16, ENC_NA);
	offset += 16;

}

static gboolean
dissect_wow(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
	gint8 size_field_offset = -1;
	guint8 cmd;

	cmd = tvb_get_guint8(tvb, 0);

	if(WOW_SERVER_TO_CLIENT && cmd == REALM_LIST)
		size_field_offset = 1;
	if(WOW_CLIENT_TO_SERVER && cmd == AUTH_LOGON_CHALLENGE)
		size_field_offset = 2;

	if(size_field_offset > -1) {
		tcp_dissect_pdus(tvb, pinfo, tree, wow_preference_desegment,
				 size_field_offset+2, get_wow_pdu_len,
				 dissect_wow_pdu, data);

	} else {
		/* Doesn't have a size field, so it cannot span multiple
		   segments.  Therefore, dissect this packet normally. */
		dissect_wow_pdu(tvb, pinfo, tree, data);
	}

	return TRUE;
}

static void
parse_logon_proof(tvbuff_t *tvb, packet_info* pinfo, proto_tree *wow_tree, guint32 offset) {
	guint8 error, two_factor_enabled;

	if (WOW_CLIENT_TO_SERVER) {
		proto_tree_add_item(wow_tree, hf_wow_logon_proof_client_to_server.srp_a, tvb,
				    offset, 32, ENC_NA);
		offset += 32;

		proto_tree_add_item(wow_tree, hf_wow_logon_proof_client_to_server.srp_m1,
				    tvb, offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_logon_proof_client_to_server.crc_hash,
				    tvb, offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_num_keys,
				    tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		if (!version_is_at_or_above(1, 12, 0)) {
			return;
		}
		two_factor_enabled = tvb_get_guint8(tvb, offset);
		proto_tree_add_item(wow_tree, hf_wow_two_factor_enabled, tvb,
				    offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		if (!two_factor_enabled) {
			return;
		}

		proto_tree_add_item(wow_tree, hf_wow_two_factor_pin_salt, tvb,
				    offset, 16, ENC_NA);
		offset += 16;

		proto_tree_add_item(wow_tree, hf_wow_logon_proof_client_to_server.pin_hash, tvb,
				    offset, 20, ENC_NA);
		offset += 20;

	} else if (WOW_SERVER_TO_CLIENT) {
		error = tvb_get_guint8(tvb, offset);
		proto_tree_add_item(wow_tree, hf_wow_error, tvb,
				    offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;
		if (error != SUCCESS) {
			// Following fields are only present when not an error.
			return;
		}

		proto_tree_add_item(wow_tree, hf_wow_logon_proof_server_to_client.srp_m2,
				    tvb, offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_logon_proof_server_to_client.hardware_survey_id,
				    tvb, offset, 4, ENC_LITTLE_ENDIAN);
		offset += 4;
	}
}
static void
parse_realm_list_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset,
				  proto_tree *wow_realms_tree) {
	guint8 num_realms, ii;
	gchar *string, *realm_name;
	gint len;

	proto_tree_add_item(wow_tree, hf_wow_pkt_size,
			    tvb, offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

	offset += 4; /* Unknown field; always 0 */

	proto_tree_add_item(wow_tree, hf_wow_num_realms,
			    tvb, offset, 1, ENC_LITTLE_ENDIAN);
	num_realms = tvb_get_guint8(tvb, offset);
	offset += 1;

	for(ii = 0; ii < num_realms; ii++) {
		realm_name = tvb_get_stringz_enc(wmem_packet_scope(), tvb,
						 offset + 5,
						 &len, ENC_ASCII);

		wow_realms_tree = proto_tree_add_subtree(wow_tree, tvb,
							 offset, 0,
							 ett_wow_realms, NULL,
							 realm_name);

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_type, tvb, offset, 4, ENC_LITTLE_ENDIAN);
		offset += 4;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_flags, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_string(wow_realms_tree, hf_wow_realm_name, tvb, offset, len, realm_name);
		offset += len;

		string = tvb_get_stringz_enc(wmem_packet_scope(), tvb, offset,
					     &len, ENC_ASCII);
		proto_tree_add_string(wow_realms_tree, hf_wow_realm_socket, tvb, offset, len, string);
		offset += len;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_population_level, tvb, offset, 4, ENC_LITTLE_ENDIAN);
		offset += 4;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_num_characters, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_category, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_id, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

	}

	/* Footer is always 2 bytes */
	offset += 2;

}


void
proto_register_wow(void)
{
	module_t *wow_module; /* For our preferences */

    static hf_register_info hf[] = {
            { &hf_wow_command,
		  { "Command", "wow.cmd",
		    FT_UINT8, BASE_HEX, VALS(cmd_vs), 0,
		    "Type of packet", HFILL }
		},
		{ &hf_wow_protocol_version,
		  { "Protocol version", "wow.protocol_version",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Version of packet", HFILL }
		},
		{ &hf_wow_error,
		  { "Error", "wow.error",
		    FT_UINT8, BASE_HEX, VALS(error_vs), 0,
		    NULL, HFILL }
		},
		{ &hf_wow_pkt_size,
		  { "Packet size", "wow.pkt_size",
		    FT_UINT16, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.gamename,
		  { "Game name", "wow.gamename",
		    FT_STRING, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_version.major,
		  { "Version 1", "wow.version1",
		    FT_UINT8, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_version.minor,
		  { "Version 2", "wow.version2",
		    FT_UINT8, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_version.patch,
		  { "Version 3", "wow.version3",
		    FT_UINT8, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_version.revision,
		  { "Build", "wow.build",
		    FT_UINT16, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.platform,
		  { "Platform", "wow.platform",
		    FT_STRING, BASE_NONE, 0, 0,
		    "CPU architecture of client system", HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.os,
		  { "Operating system", "wow.os",
		    FT_STRING, BASE_NONE, 0, 0,
		    "Operating system of client system", HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.country,
		  { "Country", "wow.country",
		    FT_STRING, BASE_NONE, 0, 0,
		    "Language and country of client system", HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.timezone_bias,
		  { "Timezone bias", "wow.timezone_bias",
		    FT_UINT32, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.ip,
		  { "IP address", "wow.ip",
		    FT_IPv4, BASE_NONE, 0, 0,
		    "Client's actual IP address", HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.srp_i_len,
		  { "SRP I length", "wow.srp.i_len",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Secure Remote Password protocol 'I' value length", HFILL }
		},
		{ &hf_wow_logon_challenge_client_to_server.srp_i,
		  { "SRP I", "wow.srp.i",
		    FT_STRING, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'I' value (username)", HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_b,
		  { "SRP B", "wow.srp.b",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'B' value (one of the public ephemeral values)", HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_g_len,
		  { "SRP g length", "wow.srp.g_len",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Secure Remote Password protocol 'g' value length",
		    HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_g,
		  { "SRP g", "wow.srp.g",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'g' value", HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_n_len,
		  { "SRP N length", "wow.srp.n_len",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Secure Remote Password protocol 'N' value length",
		    HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_n,
		  { "SRP N", "wow.srp.n",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'N' value (a large safe prime)", HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.srp_s,
		  { "SRP s", "wow.srp.s",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 's' (user's salt) value",
		    HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.crc_salt,
		  { "CRC salt", "wow.crc_salt",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Salt to be used for the hash in the reply packet",
		    HFILL }
		},
		{ &hf_wow_two_factor_enabled,
		  { "Two factor enabled", "wow.two_factor_enabled",
		    FT_BOOLEAN, BASE_NONE, 0, 0,
		    "Enables two factor authentication",
		    HFILL }
		},
		{ &hf_wow_logon_proof_client_to_server.srp_a,
		  { "SRP A", "wow.srp.a",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'A' value (one of the public ephemeral values)", HFILL }
		},
		{ &hf_wow_logon_proof_client_to_server.srp_m1,
		  { "SRP M1", "wow.srp.m1",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'M1' value", HFILL }
		},
		{ &hf_wow_logon_proof_client_to_server.crc_hash,
		  { "CRC hash", "wow.crc_hash",
		    FT_BYTES, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_num_keys,
		  { "Number of keys", "wow.num_keys",
		    FT_UINT8, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_proof_server_to_client.hardware_survey_id,
		  { "Hardware Survey ID", "wow.hardware_survey_id",
		    FT_UINT32, BASE_DEC, 0, 0,
		    "ID of a hardware survey that the client should run", HFILL }
		},
		{ &hf_wow_logon_proof_server_to_client.srp_m2,
		  { "SRP M2", "wow.srp.m2",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Secure Remote Password protocol 'M2' value", HFILL }
		},
		{ &hf_wow_challenge_data,
		  { "Reconnection Challenge Data", "wow.reconnect_challenge_data",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Random data used for reconnection calculation", HFILL }
		},
		{ &hf_wow_reconnect_challenge_server_to_client.checksum_salt,
		  { "Reconnection Checksum Salt", "wow.reconnect_checksum_salt",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Unknown. Unused in 1.12", HFILL }
		},
		{ &hf_wow_client_proof,
		  { "Reconnection Client Proof", "wow.reconnect_proof",
		    FT_BYTES, BASE_NONE, 0, 0,
		    "Client proof of knowing session key based on challenge data", HFILL }
		},
		{ &hf_wow_client_checksum,
		  { "Reconnection Checksum", "wow.reconnect_checksum",
		    FT_BYTES, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_challenge_server_to_client.pin_grid_seed,
		  { "Two Factor PIN Grid Seed", "wow.two_factor_pin_grid_seed",
		    FT_UINT32, BASE_HEX, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_two_factor_pin_salt,
		  { "Two Factor PIN Salt", "wow.two_factor_pin_salt",
		    FT_BYTES, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_logon_proof_client_to_server.pin_hash,
		  { "Two Factor PIN Hash", "wow.two_factor_pin_hash",
		    FT_BYTES, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_num_realms,
		  { "Number of realms", "wow.num_realms",
		    FT_UINT16, BASE_DEC, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_realm_type,
		  { "Type", "wow.realm_type",
		    FT_UINT8, BASE_DEC, VALS(realm_type_vs), 0,
		    "Also known as realm icon", HFILL }
		},
		{ &hf_wow_realm_flags,
		  { "Status", "wow.realm_flags",
		    FT_UINT8, BASE_DEC, VALS(realm_flags_vs), 0,
		    NULL, HFILL }
		},
		{ &hf_wow_realm_category,
		  { "Category", "wow.realm_category",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Language category the realm should be shown in", HFILL }
		},
		{ &hf_wow_realm_name,
		  { "Name", "wow.realm_name",
		    FT_STRINGZ, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_realm_socket,
		  { "Server socket", "wow.realm_socket",
		    FT_STRINGZ, BASE_NONE, 0, 0,
		    "IP address and port to connect to on the server to reach this realm", HFILL }
		},
		{ &hf_wow_realm_population_level,
		  { "Population level", "wow.realm_population_level",
		    FT_FLOAT, BASE_NONE, 0, 0,
		    NULL, HFILL }
		},
		{ &hf_wow_realm_num_characters,
		  { "Number of characters", "wow.realm_num_characters",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Number of characters the user has in this realm", HFILL }
		},
		{ &hf_wow_realm_id,
		  { "Realm id", "wow.realm_id",
		    FT_UINT8, BASE_DEC, 0, 0,
		    "Used for initial sorting the in client menu", HFILL }
		}
	};

	static gint *ett[] = {
		&ett_wow,
		&ett_wow_realms
	};

	proto_wow = proto_register_protocol("World of Warcraft",
					    "WOW", "wow");

	proto_register_field_array(proto_wow, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	wow_module = prefs_register_protocol(proto_wow, NULL);

	prefs_register_bool_preference(wow_module, "desegment", "Reassemble wow messages spanning multiple TCP segments.", "Whether the wow dissector should reassemble messages spanning multiple TCP segments.  To use this option, you must also enable \"Allow subdissectors to reassemble TCP streams\" in the TCP protocol settings.", &wow_preference_desegment);

}

void
proto_reg_handoff_wow(void)
{
	dissector_handle_t wow_handle;

	wow_handle = create_dissector_handle(dissect_wow, proto_wow);
	dissector_add_uint_with_preference("tcp.port", WOW_PORT, wow_handle);

}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
