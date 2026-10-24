/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

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

#include "Network.hpp"

#include "../Utilities.hpp"
#include "../Constants.hpp"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>

Network::Network(int port) //Constructor
{

    this->port = port;
    now = 0;

    //start networking
    if (enet_initialize () != 0) {
        //fprintf(stderr, "An error occurred while initializing ENet.\n");
        exit (EXIT_FAILURE);
    }

    client = enet_host_create (NULL /* create a client host */,
    32 /* Allow up to 10 outgoing connections */, //Todo: Should this be configurable?
    0 /* allow maximum number of channels */,
    0 /* unlimited bandwidth */,
    0 /* unlimited bandwidth */);
    if (client == NULL) {
        std::cout << "An error occurred while trying to create an ENet client host." << std::endl;
        exit (EXIT_FAILURE);
    }

    std::cout << "Started enet\n";

    //TODO: Think if this is the best way to handle failure
}

Network::~Network() //Destructor
{
    //shut down networking
    enet_host_destroy(client);
    enet_deinitialize();

    std::cout << "Shut down enet\n";
}

void Network::connectToServer(std::string hostnames, bool keepAll)
{

    //hostname may be multiple comma separated names
    std::vector<std::string> multipleHostnames = Utilities::split(hostnames,',');

    //Ensure there's at least one entry
    if (multipleHostnames.size() < 1 ) {
        multipleHostnames.push_back(""); //Add an empty record
    }

    //Set up a peer for each hostname
    for (unsigned int i = 0; i<multipleHostnames.size(); i++) {
        Slot slot;
        std::string thisHostname = Utilities::trim(multipleHostnames.at(i));
        slot.name = thisHostname;

        //Check if the string contains a ':', and if so, split into hostname and port part
        if (thisHostname.find(':') != std::string::npos) {
            std::vector<std::string> splitHostname = Utilities::split(thisHostname,':');
            if (splitHostname.size()==2) {
                thisHostname = splitHostname.at(0);
                slot.address.port = Utilities::lexical_cast<enet_uint16>(splitHostname.at(1));
            } else {
                slot.address.port = port; //Fall back to default
            }
        } else {
            slot.address.port = port;

            //Count number of instances of this earlier in list, and if so, increment port, so
            //localhost,localhost,localhost would become like localhost:port,localhost:port+1,localhost:port+2
            for (unsigned int j=0; j<i; j++) {
                if (thisHostname.compare(Utilities::trim(multipleHostnames.at(j)))==0) {
                    slot.address.port++;
                }
            }
        }

        //A name that cannot be resolved must not fall back on whatever address was left over (that
        //connected a mistyped station to the previous one a second time).
        if (enet_address_set_host (& slot.address, thisHostname.c_str()) != 0) {
            std::cout << "ENet connection failed to:" << thisHostname << " (name not found)" << std::endl;
            unreachedNames.push_back(thisHostname);
            if (keepAll) { slots.push_back(slot); }
            continue;
        }
        slot.resolvable = true;

        /* Initiate the connection, allocating the maximum number of channels. */
        ENetPeer* peer = enet_host_connect (client, & slot.address, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0);

        if (peer == NULL)
        {
            std::cout << "No available peers for initiating an ENet connection." << std::endl;
            exit (EXIT_FAILURE);
        }
        /* Wait up to 1 second for the connection attempt to succeed. */
        if (enet_host_service (client, & event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT && event.peer == peer) {
            std::cout << "ENet connection succeeded to: " << thisHostname << std::endl;
            slot.peer = peer;
            slot.connected = true;
            slots.push_back(slot);
        } else {
            /* Either the 1 second is up or a disconnect event was */
            /* received. Reset the peer in the event the 1 second */
            /* had run out without any significant event. */
            enet_peer_reset (peer);
            std::cout << "ENet connection failed to:" << thisHostname << (keepAll ? " (will try again)" : "") << std::endl;
            if (keepAll) { slots.push_back(slot); }
        }
    }
}

unsigned int Network::getNumberOfPeers()
{
    return slots.size();
}

std::string Network::getPeerName(unsigned int peerNumber) const
{
    return peerNumber < slots.size() ? slots[peerNumber].name : std::string();
}

bool Network::isConnected(unsigned int peerNumber) const
{
    return peerNumber < slots.size() && slots[peerNumber].connected;
}

void Network::sendString(std::string stringToSend, bool reliable, unsigned int peerNumber)
{
    if (peerNumber < slots.size() && slots[peerNumber].connected && slots[peerNumber].peer) {

        int reliableFlag;
        if(reliable) {
            reliableFlag = ENET_PACKET_FLAG_RELIABLE;
        } else {
            reliableFlag = 0;
        }

        if (stringToSend.length() > 0) {
            ENetPacket * packet = enet_packet_create (stringToSend.c_str(),
            strlen (stringToSend.c_str()) + 1,
            reliableFlag); //Flag

            // Send the packet to peer over channel id 0.
            enet_peer_send(slots[peerNumber].peer, 0, packet);

            // One could just use enet_host_service() instead.
            enet_host_flush (client);
        }
    }
}

void Network::listenForMessages()
{
    while (enet_host_service (client, & event, 10) > 0) {
        //Which station, if any, it is about
        int which = -1;
        for (unsigned int i = 0; i < slots.size(); i++) {
            if (slots[i].peer && event.peer == slots[i].peer) { which = (int)i; }
        }
        if (event.type==ENET_EVENT_TYPE_RECEIVE) {
            //The whole packet (no length limit), and never past its end even without a final 0
            std::string receivedString((const char*)event.packet->data, strnlen((const char*)event.packet->data, event.packet->dataLength));
            if (which >= 0) {
                slots[which].latest = receivedString;
                slots[which].lastMessageMs = now;
                slots[which].heard = true;
            }
            enet_packet_destroy (event.packet);
        }
        else if (event.type == ENET_EVENT_TYPE_CONNECT && which >= 0 && !slots[which].connected) {
            std::cout << "ENet connection succeeded to: " << slots[which].name << " (joined)" << std::endl;
            slots[which].connected = true;
            slots[which].heard = false;
            slots[which].latest.clear();
            slots[which].lastMessageMs = now;
            joined.push_back((unsigned int)which);
        }
        else if (event.type == ENET_EVENT_TYPE_DISCONNECT && which >= 0) {
            std::cout << "ENet: " << slots[which].name << " disconnected" << std::endl;
            const bool wasConnected = slots[which].connected;
            slots[which].peer = 0;   //(ENet has already released it)
            slots[which].connected = false;
            slots[which].heard = false;
            slots[which].latest.clear();
            slots[which].attemptMs = now;
            if (wasConnected) { lost.push_back((unsigned int)which); }
        }
    }
}

void Network::dropSlot(unsigned int i, bool tellLost)
{
    Slot& s = slots[i];
    if (s.peer) { enet_peer_reset(s.peer); }
    s.peer = 0;
    if (s.connected && tellLost) { lost.push_back(i); }
    s.connected = false;
    s.heard = false;
    s.latest.clear();
    s.attemptMs = now;
}

void Network::maintain(unsigned long long nowMs)
{
    now = nowMs;
    const unsigned long long silentLimit = 6000, retryEvery = 3000, attemptLimit = 2500;
    for (unsigned int i = 0; i < slots.size(); i++) {
        Slot& s = slots[i];
        //Answers every update: silent for seconds (closed, crashed, cable out) means gone. A
        //simulator still loading its scenario has not answered yet, and is left to load.
        if (s.connected && s.heard && now - s.lastMessageMs > silentLimit) {
            std::cout << "ENet: " << s.name << " silent, dropped" << std::endl;
            dropSlot(i, true);
            continue;
        }
        //A connection asked for and not answered: given up, to be asked again later
        if (s.peer && !s.connected && now - s.attemptMs > attemptLimit) {
            dropSlot(i, false);
            continue;
        }
        //Not connected: try again, without waiting for the answer (it comes as a CONNECT event)
        if (!s.peer && s.resolvable && now - s.attemptMs > retryEvery) {
            s.peer = enet_host_connect(client, &s.address, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0);
            s.attemptMs = now;
        }
    }
}

std::vector<unsigned int> Network::takeJoined()
{
    std::vector<unsigned int> out;
    out.swap(joined);
    return out;
}

std::vector<unsigned int> Network::takeLost()
{
    std::vector<unsigned int> out;
    out.swap(lost);
    return out;
}

std::string Network::getLatestMessage(unsigned int peerNumber)
{
    if (peerNumber < slots.size()) {
        return slots[peerNumber].latest;
    } else {
        return "";
    }
}
