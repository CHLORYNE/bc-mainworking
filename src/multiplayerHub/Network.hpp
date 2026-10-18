/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2016 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#ifndef __NETWORK_HPP_INCLUDED__
#define __NETWORK_HPP_INCLUDED__

#include <string>
#include <vector>

#include <enet/enet.h>

class Network
{
public:
    Network(int port);
    ~Network();
    //Connects to each comma-separated host (up to a second each). With keepAll, a host that does
    //not answer keeps its place (its number, and so its ship) and is tried again by maintain();
    //without it, only the hosts that answered are kept.
    void connectToServer(std::string hostnames, bool keepAll = false);
    unsigned int getNumberOfPeers();
    std::string getPeerName(unsigned int peerNumber) const; //station name as typed, for station N
    const std::vector<std::string>& getUnreachedNames() const { return unreachedNames; } //names that could not be resolved
    bool isConnected(unsigned int peerNumber) const;

    void sendString(std::string stringToSend, bool reliable, unsigned int peerNumber);
    void listenForMessages();
    std::string getLatestMessage(unsigned int peerNumber);

    //Every few seconds: a station silent for too long is dropped; a station not connected is
    //tried again (without waiting). nowMs: any millisecond clock.
    void maintain(unsigned long long nowMs);
    //Stations that have (re)connected / been lost since the last call
    std::vector<unsigned int> takeJoined();
    std::vector<unsigned int> takeLost();

private:
    struct Slot
    {
        std::string name;
        ENetAddress address;
        bool resolvable = false;
        ENetPeer* peer = 0;          //connected, or a connection under way
        bool connected = false;
        unsigned long long attemptMs = 0;
        unsigned long long lastMessageMs = 0;
        bool heard = false;          //has sent something since it connected
        std::string latest;
    };
    void dropSlot(unsigned int i, bool tellLost);

    int port;
    ENetHost* client; //One client
    ENetEvent event;
    std::vector<Slot> slots;
    std::vector<std::string> unreachedNames;
    std::vector<unsigned int> joined, lost;
    unsigned long long now;
};

#endif
