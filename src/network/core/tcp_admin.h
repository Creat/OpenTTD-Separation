/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_admin.h Basic functions to receive and send TCP packets to and from the admin network.
 */

#ifndef NETWORK_CORE_TCP_ADMIN_H
#define NETWORK_CORE_TCP_ADMIN_H

#include "os_abstraction.h"
#include "tcp.h"
#include "../network_type.h"
#include "../../core/pool_type.hpp"

#ifdef ENABLE_NETWORK

/**
 * Enum with types of TCP packets specific to the admin network.
 * This protocol may only be extended to ensure stability.
 */
enum PacketAdminType {
	ADMIN_PACKET_ADMIN_JOIN,             ///< The admin announces and authenticates itself to the server.
	ADMIN_PACKET_ADMIN_QUIT,             ///< The admin tells the server that it is quitting.
	ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY, ///< The admin tells the server the update frequency of a particular piece of information.
	ADMIN_PACKET_ADMIN_POLL,             ///< The admin explicitly polls for a piece of information.

	ADMIN_PACKET_SERVER_FULL = 100,      ///< The server tells the admin it cannot accept the admin.
	ADMIN_PACKET_SERVER_BANNED,          ///< The server tells the admin it is banned.
	ADMIN_PACKET_SERVER_ERROR,           ///< The server tells the admin an error has occurred.
	ADMIN_PACKET_SERVER_PROTOCOL,        ///< The server tells the admin its protocol version.
	ADMIN_PACKET_SERVER_WELCOME,         ///< The server welcomes the admin to a game.
	ADMIN_PACKET_SERVER_NEWGAME,         ///< The server tells the admin its going to start a new game.
	ADMIN_PACKET_SERVER_SHUTDOWN,        ///< The server tells the admin its shutting down.

	ADMIN_PACKET_SERVER_DATE,            ///< The server tells the admin what the current game date is.
	ADMIN_PACKET_SERVER_CLIENT_JOIN,     ///< The server tells the admin that a client has joined.
	ADMIN_PACKET_SERVER_CLIENT_INFO,     ///< The server gives the admin information about a client.
	ADMIN_PACKET_SERVER_CLIENT_UPDATE,   ///< The server gives the admin an information update on a client.
	ADMIN_PACKET_SERVER_CLIENT_QUIT,     ///< The server tells the admin that a client quit.
	ADMIN_PACKET_SERVER_CLIENT_ERROR,    ///< The server tells the admin that a client caused an error.
	ADMIN_PACKET_SERVER_COMPANY_NEW,     ///< The server tells the admin that a new company has started.
	ADMIN_PACKET_SERVER_COMPANY_INFO,    ///< The server gives the admin information about a company.
	ADMIN_PACKET_SERVER_COMPANY_UPDATE,  ///< The server gives the admin an information update on a company.
	ADMIN_PACKET_SERVER_COMPANY_REMOVE,  ///< The server tells the admin that a company was removed.

	INVALID_ADMIN_PACKET = 0xFF,         ///< An invalid marker for admin packets.
};

/** Status of an admin. */
enum AdminStatus {
	ADMIN_STATUS_INACTIVE,      ///< The admin is not connected nor active.
	ADMIN_STATUS_ACTIVE,        ///< The admin is active.
	ADMIN_STATUS_END            ///< Must ALWAYS be on the end of this list!! (period)
};

/** Update types an admin can register a frequency for */
enum AdminUpdateType {
	ADMIN_UPDATE_DATE,            ///< Updates about the date of the game.
	ADMIN_UPDATE_CLIENT_INFO,     ///< Updates about the information of clients.
	ADMIN_UPDATE_COMPANY_INFO,    ///< Updates about the generic information of companies.
	ADMIN_UPDATE_END              ///< Must ALWAYS be on the end of this list!! (period)
};

/** Update frequencies an admin can register. */
enum AdminUpdateFrequency {
	ADMIN_FREQUENCY_POLL      = 0x01, ///< The admin can poll this.
	ADMIN_FREQUENCY_DAILY     = 0x02, ///< The admin gets information about this on a daily basis.
	ADMIN_FREQUENCY_WEEKLY    = 0x04, ///< The admin gets information about this on a weekly basis.
	ADMIN_FREQUENCY_MONTHLY   = 0x08, ///< The admin gets information about this on a monthly basis.
	ADMIN_FREQUENCY_QUARTERLY = 0x10, ///< The admin gets information about this on a quarterly basis.
	ADMIN_FREQUENCY_ANUALLY   = 0x20, ///< The admin gets information about this on a yearly basis.
	ADMIN_FREQUENCY_AUTOMATIC = 0x40, ///< The admin gets information about this when it changes.
};
DECLARE_ENUM_AS_BIT_SET(AdminUpdateFrequency);

/** Reasons for removing a company - communicated to admins. */
enum AdminCompanyRemoveReason {
	ADMIN_CRR_MANUAL,    ///< The company is manually removed.
	ADMIN_CRR_AUTOCLEAN, ///< The company is removed due to autoclean.
	ADMIN_CRR_BANKRUPT   ///< The company went belly-up.
};

#define DECLARE_ADMIN_RECEIVE_COMMAND(type) virtual NetworkRecvStatus NetworkPacketReceive_## type ##_command(Packet *p)
#define DEF_ADMIN_RECEIVE_COMMAND(cls, type) NetworkRecvStatus cls ##NetworkAdminSocketHandler::NetworkPacketReceive_ ## type ## _command(Packet *p)

/** Main socket handler for admin related connections. */
class NetworkAdminSocketHandler : public NetworkTCPSocketHandler {
protected:
	char admin_name[NETWORK_CLIENT_NAME_LENGTH];           ///< Name of the admin.
	char admin_version[NETWORK_REVISION_LENGTH];           ///< Version string of the admin.
	AdminStatus status;                                    ///< Status of this admin.

	/**
	 * Join the admin network:
	 * string  Password the server is expecting for this network.
	 * string  Name of the application being used to connect.
	 * string  Version string of the application being used to connect.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_JOIN);

	/**
	 * Notification to the server that this admin is quitting.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_QUIT);

	/**
	 * Register updates to be sent at certain frequencies (as announced in the PROTOCOL packet):
	 * uint16  Update type (see #AdminUpdateType).
	 * uint16  Update frequency (see #AdminUpdateFrequency), setting #ADMIN_FREQUENCY_POLL is always ignored.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY);

	/**
	 * Poll the server for certain updates, an invalid poll (e.g. not existent id) gets silently dropped:
	 * uint8   #AdminUpdateType the server should answer for, only if #AdminUpdateFrequency #ADMIN_FREQUENCY_POLL is advertised in the PROTOCOL packet.
	 * uint32  ID relevant to the packet type, e.g.
	 *          - the client ID for #ADMIN_UPDATE_CLIENT_INFO. Use UINT32_MAX to show all clients.
	 *          - the company ID for #ADMIN_UPDATE_COMPANY_INFO. Use UINT32_MAX to show all companies.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_POLL);

	/**
	 * The server is full (connection gets closed).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_FULL);

	/**
	 * The source IP address is banned (connection gets closed).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_BANNED);

	/**
	 * An error was caused by this admin connection (connection gets closed).
	 * uint8  NetworkErrorCode the error caused.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_ERROR);

	/**
	 * Inform a just joined admin about the protocol specifics:
	 * uint8   Protocol version.
	 * bool    Further protocol data follows (repeats through all update packet types).
	 * uint16  Update packet type.
	 * uint16  Frequencies allowed for this update packet (bitwise).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_PROTOCOL);

	/**
	 * Welcome a connected admin to the game:
	 * string  Name of the Server (e.g. as advertised to master server).
	 * string  OpenTTD version string.
	 * bool    Server is dedicated.
	 * string  Name of the Map.
	 * uint32  Random seed of the Map.
	 * uint8   Landscape of the Map.
	 * uint32  Start date of the Map.
	 * uint16  Map width.
	 * uint16  Map height.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_WELCOME);

	/**
	 * Notification about a newgame.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_NEWGAME);

	/**
	 * Notification about the server shutting down.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_SHUTDOWN);

	/**
	 * Send the current date of the game:
	 * uint32  Current game date.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_DATE);

	/**
	 * Notification of a new client:
	 * uint32  ID of the new client.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_JOIN);

	/**
	 * Client information of a specific client:
	 * uint32  ID of the client.
	 * string  Network address of the client.
	 * string  Name of the client.
	 * uint8   Language of the client.
	 * uint32  Date the client joined the game.
	 * uint8   ID of the company the client is playing as (255 for spectators).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_INFO);

	/**
	 * Client update details on a specific client (e.g. after rename or move):
	 * uint32  ID of the client.
	 * string  Name of the client.
	 * uint8   ID of the company the client is playing as (255 for spectators).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_UPDATE);

	/**
	 * Notification about a client leaving the game.
	 * uint32  ID of the client that just left.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_QUIT);

	/**
	 * Notification about a client error (and thus the clients disconnection).
	 * uint32  ID of the client that made the error.
	 * uint8   Error the client made (see NetworkErrorCode).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_ERROR);

	/**
	 * Notification of a new company:
	 * uint8   ID of the new company.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_NEW);

	/**
	 * Company information on a specific company:
	 * uint8   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8   Main company colour.
	 * bool    Company is password protected.
	 * uint32  Year the company was inaugurated.
	 * bool    Company is an AI.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_INFO);

	/**
	 * Company information of a specific company:
	 * uint8   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8   Main company colour.
	 * bool    Company is password protected.
	 * uint8   Quarters of bankruptcy.
	 * uint8   Owner of share 1.
	 * uint8   Owner of share 2.
	 * uint8   Owner of share 3.
	 * uint8   Owner of share 4.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_UPDATE);

	/**
	 * Notification about a removed company (e.g. due to banrkuptcy).
	 * uint8   ID of the company.
	 * uint8   Reason for being removed (see #AdminCompanyRemoveReason).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_REMOVE);

	NetworkRecvStatus HandlePacket(Packet *p);
public:
	NetworkRecvStatus CloseConnection(bool error = true);

	NetworkAdminSocketHandler(SOCKET s);
	~NetworkAdminSocketHandler();

	NetworkRecvStatus Recv_Packets();

	const char *Recv_Command(Packet *p, CommandPacket *cp);
	void Send_Command(Packet *p, const CommandPacket *cp);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_ADMIN_H */