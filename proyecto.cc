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

En este código se simula el proceso de definición de un esquema 
criptográfico para la comunicación de una red de sensores inalámbrica.

EL proceso tiene varias etapas:

FASE DE PRE DISTRIBUCIÓN DE LLAVES: A cada nodo se le asignan un paquete de llaves
únicas entre sí. Dos nodos pueden comunicarse entre sí solo si comparten al menos una llave.

FASE DE DESCUBRIMIENTO DE LLAVES COMPARTIDAS: Cada nodo hace una transmición de broadcast para 
saber con qué nodos puede comunicarse. COmo no todos los nodos tendrán llaves compartidas, no
habrá enlace directo entre cada par de nodos, por ello se tiene la siguiente etapa.





Parámetros de consola que pueden personalizar la ejecucion del programa

./waf --run "scratch/proyecto --numPackets=10" --vis
./waf --run "scratch/proyecto --numPackets=30 --numNodes=20" --vis
./waf --run "scratch/proyecto --numPackets=30 --x_limit=10 --y_limit=10" --vis
./waf --run "scratch/proyecto --numNodes=6 --pool_size=10 --num_keys_node=2" --vis


*/




#include <iostream>
#include <string>
#include "ns3/network-module.h"
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

//Definicion de variables globales
std::vector<std::vector<std::string>> nodeKeys;
std::vector<std::string> pool;
uint32_t direct_link_count = 0;




void SendStuff (Ptr<Socket> sock, InetSocketAddress destiny,std::string content)
{
  //Prepare message to send
  std::ostringstream msg; msg << content << '\0';
  //Create packet
  Ptr<Packet> p = Create<Packet> ((uint8_t*) msg.str().c_str(), msg.str().length()+1);
  p->AddPaddingAtEnd (100);
  //Send packet
  sock->SendTo (p, 0, destiny);
  return;
}




void checkSharedKey (Ptr<Socket> socket)
{
  uint32_t id_of_reciving_node = socket->GetNode()->GetId();
  //getting ip addres of sender node
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  //preparing received packet for data extraction
  packet->RemoveAllPacketTags ();
  packet->RemoveAllByteTags ();
  //Extraction of packet content
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData(buffer, packet->GetSize ());
  std::string pckContent = std::string(buffer, buffer+packet->GetSize());
  NS_LOG_INFO("Node "<<id_of_reciving_node<<" received a message from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
  NS_LOG_INFO ("Message Received: " << pckContent );
  //contructing the keys received
  std::vector<std::string> keyIdsRecvd = decodeKeyIds(pckContent);
  //cheking for shared keys
  for (size_t i = 0; i < keyIdsRecvd.size(); i++)
  {
    //std::cout<<keyIdsRecvd[i]<<" ";
    std::string key = pool[i];
    if(getIndex(nodeKeys[id_of_reciving_node],key)!=-1){
      NS_LOG_INFO ("Direct link made: ");
      direct_link_count++;
      break;
    }
  }
  //std::cout<<std::endl;
  std::cout<<direct_link_count<<std::endl;
  

}

void discoveryPhaseResults(){

  NS_LOG_INFO("************************************************************");
  double totalEdges = (double)numNodes*(numNodes - 1)/(double)2;
  NS_LOG_INFO("Of "<<totalEdges<<" direct links possible, "<<direct_link_count/2<<" where made");
  NS_LOG_INFO("************************************************************");
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
    pool_size = 1000;
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
    pool = generatePool(pool_size);


    //EN un vector se guardan las k llaves que le corresponden a cada nodo
    nodeKeys = assignKeysToNodes(pool,pool_size, numNodes, num_keys_node);

    //Se imprimen las llaves de cada nodo (esto se puede quitar despues)
    for (size_t i = 0; i < nodeKeys.size(); i++)
    {
      NS_LOG_INFO("Node "<<i);
      for (size_t j = 0; j < nodeKeys[i].size(); j++)
      {
        NS_LOG_INFO(nodeKeys[i][j]);
      }
      
    }
    
   /////////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //                          DESPLIEGUE DE LA RED                          //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////
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
    Ipv4InterfaceContainer addressContainer = ipAddrs.Assign (mainDeviceContainer);
    
    //La red de sensores no tiene movilidad, simplemente se configura cada nodo en una posición
    //aleatoria dentro de un rectangulo definido por x_limit y y_limit
    NS_LOG_INFO("SETTING UP POSSITIONS OF NODES.");
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

   /////////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //             FASE DE DESCUBRIMIENTO DE LLAVES COMPARTIDAS               //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////
  
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  //Se ponen a escuchar a todos los nodos
  for (uint32_t i = 0; i < nodeContainer.GetN (); ++i)
  {
    Ptr<Socket> destiny = Socket::CreateSocket (nodeContainer.Get (i), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
    destiny->Bind (local);
    destiny->SetRecvCallback (MakeCallback (&checkSharedKey));
  }
  //Cada uno de los nodos hace una emisión de broadcast para informar a sus vecinos que llaves
  //tiene
  for (uint32_t i = 0; i < nodeContainer.GetN (); ++i)
  {
    Ptr<Socket> source = Socket::CreateSocket (nodeContainer.Get (i), tid);
    InetSocketAddress remote = InetSocketAddress (Ipv4Address ("192.168.0.255"), 80);
    source->SetAllowBroadcast (true);
    source->Connect (remote);
    std::string codedKeyIds = encodeKeyIds(pool, nodeKeys[i]);
    Simulator::Schedule (Seconds (i),&SendStuff, source, remote, codedKeyIds);
  }
  Simulator::Schedule (Seconds (nodeContainer.GetN()),&discoveryPhaseResults);

  
  

  


  Simulator::Stop (Seconds (30));

  AnimationInterface pAnim ("testProyecto.xml");


  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
  }
