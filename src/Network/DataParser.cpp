///
/// Created by Anonymous275 on 4/2/2020
///

#include <string>
#include "enet.hpp"
#include <vector>
#include <iostream>
#include "../logger.h"
#include "../Settings.hpp"

void SendToAll(ENetHost *server, ENetPeer*peer, const std::string& Data,bool All, bool Reliable);
void Respond(const std::string& MSG, ENetPeer*peer);

void VehicleParser(std::string Packet,ENetPeer*peer,ENetHost*server){
    char Code = Packet.at(1);
    std::string Data = Packet.substr(3);
    switch(Code){ //Spawned Destroyed Switched/Moved Reset
        case 's':
            if(Data.at(0) == '0'){
                Packet = "Os:"+ peer->Name +":"+std::to_string(peer->serverVehicleID[0])+Packet.substr(4);
            }
            SendToAll(server,peer,Packet,true,true);
            break;
        case 'd':
            SendToAll(server,peer,Packet,true,true);
            break;
        case 'm':
            break;
        case 'r':
            SendToAll(server,peer,Packet,true,true);
            break;
    }
}

void ParseData(ENetPacket*packet, ENetPeer*peer, ENetHost*server){
    std::string Packet = (char*)packet->data;
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'p':
            Respond("p",peer);
            return;
        case 'N':
            if(SubCode == 'R')peer->Name = Packet.substr(2);
            std::cout << "Name : " << peer->Name << std::endl;
            return;
        case 'O':
            std::cout << "Received data from: " << peer->Name << " Size: " << Packet.length() << std::endl;
            VehicleParser(Packet,peer,server);
            return;
    }
    //V to Z
    std::cout << "Received data from: " << peer->Name << " Size: " << Packet.length() << std::endl;
    if(Code <= 90 && Code >= 86)SendToAll(server,peer,Packet,false,false);
    if(Debug)debug("Data : " + Packet);
}
