/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
En este codigo se definen 3 redes adhoc inalambricas
La red de mayor jerarquia sera netone
Las otras dos, nettwo, netone, solo se pueden comunicar através de netone
El numero minimo de nodos de cada red puede tener es de dos. En cada red habra almenos
una cabeza de cluster, que se encarga de comunicar


Parámetros de consola que pueden personalizar la ejecucion del programa

./waf --run "scratch/proyecto --numPackets=10" --vis
./waf --run "scratch/proyecto --numPackets=30 --numNodes=20" --vis
./waf --run "scratch/proyecto --numPackets=30 --x_limit=10 --y_limit=10" --vis
./waf --run "scratch/proyecto --numNodes=6 --pool_size=10 --num_keys_node=2" --vis


*/




#include <iostream>
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ssid.h"
#include "ns3/on-off-helper.h"
#include "ns3/qos-txop.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/netanim-module.h"
#include "key_generator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Proyecto");
//DEfinicion de parametros que se pueden modificar por consola
uint32_t packetSize; // bytes
uint32_t numPackets;
uint32_t numNodes;
uint32_t pool_size;
uint32_t num_keys_node;
double x_limit;
double y_limit;
double interval;




static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule(pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

void ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet!");
    }
}




int main (int argc, char *argv[])
{
    //DEfinicion de sistema de logs que se usara
    LogComponentEnable("Proyecto", LOG_LEVEL_INFO);

    //Definicion de valores por defecto de variables que se pueden modificar como parametros del
    //programa
    packetSize = 1000; // bytes
    numPackets = 10;
    numNodes = 60;
    x_limit = 100;
    y_limit = 100;
    interval = 1.0; // seconds
    pool_size = 100;
    num_keys_node = 15;

    //Configuracion de parametros de programa que se podran ingresar mediante ./waf ...taller --<par> = valor
    
    CommandLine cmd;
    cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue ("numPackets", "number of packets generated", numPackets);
    cmd.AddValue ("interval", "seconds betwen one package and another", interval);
    cmd.AddValue ("numNodes", "number of nodes for main network", numNodes);
    cmd.AddValue ("x_limit", "width of rectangle where nodes are going to be placed", x_limit);
    cmd.AddValue ("y_limit", "height of rectangle where nodes are going to be placed", y_limit);
    cmd.AddValue ("pool_size", "Number P of keys that form the key pool", pool_size);
    cmd.AddValue ("num_keys_node", "Number P of keys that form the key pool", num_keys_node);
    cmd.Parse (argc, argv);


    Time interPacketInterval = Seconds (interval);

    ///////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //                  FASE DE PRE DISTRIBUCIÓN DE LLAVES                    //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

    //Se genera el pool de llaves
    NS_LOG_INFO("GENERATING POOL.");
    std::vector<std::string> pool = generatePool(pool_size);


    //EN un vector se guardan las k llaves que le corresponden a cada nodo
    std::vector<std::vector<std::string>> nodeKeys = assignKeysToNodes(pool,pool_size, numNodes, num_keys_node);

    for (size_t i = 0; i < nodeKeys.size(); i++)
    {
      NS_LOG_INFO("Node 1: ");
      for (size_t j = 0; j < nodeKeys[i].size(); j++)
      {
        NS_LOG_INFO(nodeKeys[i][j]);
      }
      
    }
    
    //Creacion de la red
    NS_LOG_INFO("CREATING NODES.");
    NodeContainer nodeContainer;
    nodeContainer.Create(numNodes);

 
    //Intalacion de dispositivos de red wifi en modo adhoc, capa fisica y capa mac a los nodos
    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("OfdmRate54Mbps"));
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    NetDeviceContainer mainDeviceContainer = wifi.Install (wifiPhy, mac, nodeContainer);

    //Intalacion de pila de protocolos a los dispositivos de red
    InternetStackHelper internet;
    internet.Install (nodeContainer);
    
    //Asignacion de direcciones ip
    NS_LOG_INFO ("MAIN:setting up ip adresses on all nodes");
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
    //cada dispositivo de red tendra direcciones 192.168.0.1, 192.168.0.2....192.168.0.254
    ipAddrs.Assign (mainDeviceContainer);


    
    //La red de sensores no tiene movilidad, simplemente se configura cada nodo en una posición
    //aleatoria dentro de un rectangulo definido por x_limit y y_limit
    MobilityHelper mobility;

    std::stringstream x_rand;
    x_rand << "ns3::UniformRandomVariable[Min=0.0|Max=" << x_limit << "]";

    std::stringstream y_rand;
    y_rand << "ns3::UniformRandomVariable[Min=0.0|Max=" << y_limit << "]";

    //Primero se define la posicion de cada nodo en una grilla
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue (x_rand.str()),//Cordenada inicial en x
                                    "Y", StringValue (y_rand.str())//Cordenada inicial en Y
                                    );
    mobility.Install (nodeContainer);

   
  
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  //UN nodo va a estar escuchando en el puerto 80
  Ptr<Socket> destiny = Socket::CreateSocket (nodeContainer.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  destiny->Bind (local);
  destiny->SetRecvCallback (MakeCallback (&ReceivePacket));



  Ptr<Socket> source = Socket::CreateSocket (nodeContainer.Get (9 % (numNodes - 1)), tid);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("192.168.0.50"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);


  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &GenerateTraffic,
                                  source, packetSize, numPackets, interPacketInterval);
  Simulator::Stop (Seconds (30));

  AnimationInterface pAnim ("testProyecto.xml");


  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
  }
