//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

/*! \file stk_peer.hpp
 *  \brief Defines functions to easily manipulate 8-bit network destinated strings.
 */

#ifndef STK_PEER_HPP
#define STK_PEER_HPP

#include "network/transport_address.hpp"
#include "utils/no_copy.hpp"
#include "utils/types.hpp"

#include <enet/enet.h>

#include <atomic>
#include <memory>
#include <set>
#include <vector>

class Crypto;
class NetworkPlayerProfile;
class NetworkString;
class STKHost;
class TransportAddress;

enum PeerDisconnectInfo : unsigned int
{
    PDI_TIMEOUT = 0, //!< Timeout disconnected (default in enet).
    PDI_NORMAL = 1, //!< Normal disconnction with acknowledgement
    PDI_KICK = 2, //!< Kick disconnection
};   // PeerDisconnectInfo

/*! \class STKPeer
 *  \brief Represents a peer.
 *  This class is used to interface the ENetPeer structure.
 */
class STKPeer : public NoCopy
{
protected:
    /** Pointer to the corresponding ENet peer data structure. */
    ENetPeer* m_enet_peer;

    /** True if this peer is validated by server. */
    std::atomic_bool m_validated;

    /** Host id of this peer. */
    uint32_t m_host_id;

    TransportAddress m_peer_address;

    STKHost* m_host;

    std::vector<std::shared_ptr<NetworkPlayerProfile> > m_players;

    float m_connected_time;

    /** Available karts and tracks from this peer */
    std::pair<std::set<std::string>, std::set<std::string> > m_available_kts;

    std::unique_ptr<Crypto> m_crypto;

public:
    STKPeer(ENetPeer *enet_peer, STKHost* host, uint32_t host_id);
    // ------------------------------------------------------------------------
    ~STKPeer();
    // ------------------------------------------------------------------------
    void sendPacket(NetworkString *data, bool reliable = true,
                    bool encrypted = true);
    // ------------------------------------------------------------------------
    void disconnect();
    // ------------------------------------------------------------------------
    void kick();
    // ------------------------------------------------------------------------
    void reset();
    // ------------------------------------------------------------------------
    bool isConnected() const;
    const TransportAddress& getAddress() const { return m_peer_address; }
    bool isSamePeer(const STKPeer* peer) const;
    bool isSamePeer(const ENetPeer* peer) const;
    // ------------------------------------------------------------------------
    std::vector<std::shared_ptr<NetworkPlayerProfile> >& getPlayerProfiles()
                                                          { return m_players; }
    // ------------------------------------------------------------------------
    bool hasPlayerProfiles() const               { return !m_players.empty(); }
    // ------------------------------------------------------------------------
    void cleanPlayerProfiles()                           { m_players.clear(); }
    // ------------------------------------------------------------------------
    void addPlayer(std::shared_ptr<NetworkPlayerProfile> p)
                                                    { m_players.push_back(p); }
    // ------------------------------------------------------------------------
    void setValidated()                            { m_validated.store(true); }
    // ------------------------------------------------------------------------
    /** Returns if the client is validated by server. */
    bool isValidated() const                     { return m_validated.load(); }
    // ------------------------------------------------------------------------
    /** Returns the host id of this peer. */
    uint32_t getHostId() const                            { return m_host_id; }
    // ------------------------------------------------------------------------
    float getConnectedTime() const                 { return m_connected_time; }
    // ------------------------------------------------------------------------
    void setAvailableKartsTracks(std::set<std::string>& k,
                                 std::set<std::string>& t)
              { m_available_kts = std::make_pair(std::move(k), std::move(t)); }
    // ------------------------------------------------------------------------
    void eraseServerKarts(const std::set<std::string>& server_karts,
                          std::set<std::string>& karts_erase)
    {
        if (m_available_kts.first.empty())
            return;
        for (const std::string& server_kart : server_karts)
        {
            if (m_available_kts.first.find(server_kart) ==
                m_available_kts.first.end())
            {
                karts_erase.insert(server_kart);
            }
        }
    }
    // ------------------------------------------------------------------------
    void eraseServerTracks(const std::set<std::string>& server_tracks,
                           std::set<std::string>& tracks_erase)
    {
        if (m_available_kts.second.empty())
            return;
        for (const std::string& server_track : server_tracks)
        {
            if (m_available_kts.second.find(server_track) ==
                m_available_kts.second.end())
            {
                tracks_erase.insert(server_track);
            }
        }
    }
    // ------------------------------------------------------------------------
    void setPingInterval(uint32_t interval)
                            { enet_peer_ping_interval(m_enet_peer, interval); }
    // ------------------------------------------------------------------------
    uint32_t getPing() const;
    // ------------------------------------------------------------------------
    Crypto* getCrypto() const                        { return m_crypto.get(); }
    // ------------------------------------------------------------------------
    void setCrypto(std::unique_ptr<Crypto>&& c);

};   // STKPeer

#endif // STK_PEER_HPP
