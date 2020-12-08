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
./waf --run scratch/proyecto --vis
./waf --run "scratch/proyecto --numPackets=10" --vis
./waf --run "scratch/proyecto --numPackets=30 --numNodes=20" --vis
./waf --run "scratch/proyecto --numPackets=30 --x_limit=10 --y_limit=10" --vis
./waf --run "scratch/proyecto --numNodes=6 --x_limit=10 --y_limit=10" --vis
./waf --run "scratch/proyecto --numNodes=6 --pool_size=10 --num_keys_node=2" --vis
*/

#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-module.h"

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
#include "ns3/ipv4-static-routing-helper.h"
#include "key_generator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Proyecto");
//DEfinicion de parametros que se pueden modificar por consola
uint32_t numPackets;
uint32_t numNodes;
uint32_t pool_size;
uint32_t num_keys_node;
double x_limit;
double y_limit;

//Definicion de variables globales:

NetDeviceContainer mainDeviceContainer;
NodeContainer nodeContainer;
//Matriz donde la fila i tiene la lista de llaves del nodo i
std::vector<std::vector<std::string>> nodeKeys;
//Arreglo con todas las llaves
std::vector<std::string> pool;
//Arreglo de duplas (nodeA_id, nodeB_id) con la información de los nodos que tienen enlaces
//directos
std::vector<std::pair<uint32_t,uint32_t>> directLinks;
//tabla hash donde las llaves son cada uno de los id de los nodos y los valores la lista de 
//nodos con conexiones
std::map<uint32_t, std::vector<int>> linksMap;
//lista de direcciones ip
Ipv4InterfaceContainer addressContainer;
uint32_t direct_link_count = 0;
double simTime = 0;




void SendStuff (Ptr<Socket> sock, InetSocketAddress destiny,std::string content)
{
  //Preparacion del mensaje que se va a enviar
  std::ostringstream msg; msg << content << '\0';
  //Creacion del paquete
  Ptr<Packet> p = Create<Packet> ((uint8_t*) msg.str().c_str(), msg.str().length()+1);
  p->AddPaddingAtEnd (100);
  //ENvio del paquete
  sock->SendTo (p, 0, destiny);
  return;
}




void checkSharedKey (Ptr<Socket> socket)
{
  uint32_t id_of_reciving_node = socket->GetNode()->GetId();
  ////ACcediendo a direccion ip del nodo que envia
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Ipv4Address ipSEnder = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
  //Convirtiendo dirrección a string
  std::ostringstream stream;
  ipSEnder.Print(stream);
  std::string ipSenderStr =  stream.str();
  //se obtiene el id del nodo que envía
  uint32_t id_of_sender_node = getNodeId(ipSenderStr) -1 ;
  //preparando el paquete recibido para extracción de datos
  packet->RemoveAllPacketTags ();
  packet->RemoveAllByteTags ();
  //Extracción de datos del paqute
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData(buffer, packet->GetSize ());
  std::string pckContent = std::string(buffer, buffer+packet->GetSize());
  NS_LOG_INFO("Node "<<id_of_reciving_node<<" received a message from " << ipSenderStr);
  NS_LOG_INFO ("Message Received: " << pckContent );

  //Arreglo con los ids de las llaves recibidas
  std::vector<uint32_t> keyIdsRecvd = decodeKeyIds(pckContent);
  //Se verifica si se comparten llaves
  for (size_t i = 0; i < keyIdsRecvd.size(); i++)
  {
    
    std::string key = pool[keyIdsRecvd[i]];
    if(getIndex(nodeKeys[id_of_reciving_node],key)!=-1){
      NS_LOG_INFO ("Direct link made!");
      direct_link_count++;
      //Creando enlace directo
      std::pair<uint32_t,uint32_t> newPair;
      //pair: (sender, reciver)
      newPair.first = id_of_sender_node;
      newPair.second = id_of_reciving_node;
      directLinks.push_back(newPair);
      //Verificar que id_of_reciving_node exista como llave en linksMap
      if ( linksMap.find(id_of_reciving_node) == linksMap.end() ){
        //SI no, se agrega la nueva llave y una lista asociada a ella
        std::pair<uint32_t,std::vector<int>> newPair2;
        newPair2.first = id_of_reciving_node;
        newPair2.second = std::vector<int>();
        linksMap.insert(newPair2);
        //Agregando id_of_sender_node A la lista creada
        linksMap.find(id_of_reciving_node)->second.push_back(id_of_sender_node);
      } else{
        //si la llave existe, añadir id_of_sender_node a su lista
        linksMap.find(id_of_reciving_node)->second.push_back(id_of_sender_node);
    
      }
      
      break;
    }
  }  

}

void discoveryPhaseResults(){

  NS_LOG_INFO("********END OF KEY DISCOVERY PHASE *****************************");
  double totalEdges = (double)numNodes*(numNodes - 1)/(double)2;
  NS_LOG_INFO("Of "<<totalEdges<<" edges, "<<direct_link_count/2<<" where made");
  NS_LOG_INFO(direct_link_count<<" direct links made:");

  for (size_t i = 0; i < directLinks.size(); i++)
  {
    NS_LOG_INFO("Node "<<directLinks[i].first<<" with node "<<directLinks[i].second);
     
  }
  
  
NS_LOG_INFO("****************************************************************");

/*
//Impresión del mapa
std::map<uint32_t, std::vector<int>>::iterator it;
for ( it = linksMap.begin(); it != linksMap.end(); it++ )
{
    std::cout << it->first  // string (key)
              << ":" ;
    for (size_t i = 0; i < it->second.size(); i++)
    {
      std::cout << it->second[i]  // string (key)
              << "," ;
    }
    std::cout <<std::endl;
    
}
*/
}


void receive (Ptr<Socket> socket)
{
  NS_LOG_INFO("This is a secure chanel");
  uint32_t id_of_reciving_node = socket->GetNode()->GetId();
  //Accediendo a dirección ip del nodo que envia
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Ipv4Address ipSEnder = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
  //COnvirtiendo a string
  std::ostringstream stream;
  ipSEnder.Print(stream);
  std::string ipSenderStr =  stream.str();
  //Preparando paquetes recibidos para extracción de contenido
  packet->RemoveAllPacketTags ();
  packet->RemoveAllByteTags ();
  //Extracción de contenido del paquete
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData(buffer, packet->GetSize ());
  std::string pckContent = std::string(buffer, buffer+packet->GetSize());
  NS_LOG_INFO("Node "<<id_of_reciving_node<<" received a message from " << ipSenderStr);
  NS_LOG_INFO ("Message Received: " << pckContent );


}

void updateListeningFUnction(std::vector<Ptr<Socket>> listening_sockets){
  //se cambia la función que es llamada al recibir un paquete
  for (uint32_t i = 0; i < listening_sockets.size(); ++i)
  {
    Ptr<Socket> destiny = listening_sockets[i];
    destiny->SetRecvCallback (MakeCallback (&receive));

  }
  NS_LOG_INFO("NOW ITS TIME TO SEND "<<numPackets<<" PACKETS BETWEEN NODES WITH DIRECT LINKS");
}

void sendSecure(){
  //Obteniendo mensaje por consola
  NS_LOG_INFO ("Enter message: ");
  std::string message;
  std::cin >> message;
  uint32_t sender;
  
  //Obeniendo  id del nodo que envia por consola
  while(true){
    NS_LOG_INFO ("Enter sender node id: ");
    std::cin >> sender;
    if(sender<=nodeContainer.GetN()-1)
      break;
    else
      NS_LOG_INFO ("Id not valid");
  }
  
  //Obeniendo  id del nodo destino por consola
  uint32_t destiny;
  while(true){
    NS_LOG_INFO ("Enter destiny node id: ");
    std::cin >> destiny;
    if(destiny<=nodeContainer.GetN()-1)
      break;
    else
      NS_LOG_INFO ("Id not valid");
  }
  //Verifica si es posible comuniciacion segura entre nodos
  std::vector<int> validIds = linksMap.find(destiny)->second;
  if(getIndex(validIds,sender)==-1){
    NS_LOG_INFO ("These nodes dont have direct links");
    return;
  }
  //Obtiendo direccion ip de nodo destino
  Ipv4Address destineyAddress =  addressContainer.GetAddress(destiny,0);
  //Convirtiendo ip a string
  std::ostringstream stream;
  destineyAddress.Print(stream);
  std::string ipSenderStr =  stream.str();

  NS_LOG_INFO ("sending message to "<<ipSenderStr<<" after all packets are scheduled");
  //ENviando mensaje
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> source = Socket::CreateSocket (nodeContainer.Get (sender), tid);
  InetSocketAddress remote = InetSocketAddress (destineyAddress, 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);
  
  
  Simulator::Schedule (Seconds (1),&SendStuff, source, remote, message);
  //NS_LOG_INFO (sender);
}

int main (int argc, char *argv[])
{
    //DEfinicion de sistema de logs que se usara
    LogComponentEnable("Proyecto", LOG_LEVEL_INFO);

    //Definicion de valores por defecto de variables que se pueden modificar como parametros del
    //programa
    numPackets = 1;
    numNodes = 60;
    x_limit = 100;
    y_limit = 100;
    pool_size = 1000;
    num_keys_node = 15;

    //Configuracion de parametros de programa que se podran ingresar mediante ./waf ...taller --<par> = valor
    
    CommandLine cmd;
    cmd.AddValue ("numPackets", "number of packets generated", numPackets);
    cmd.AddValue ("numNodes", "number of nodes for main network", numNodes);
    cmd.AddValue ("x_limit", "width of rectangle where nodes are going to be placed", x_limit);
    cmd.AddValue ("y_limit", "height of rectangle where nodes are going to be placed", y_limit);
    cmd.AddValue ("pool_size", "Number P of keys that form the key pool", pool_size);
    cmd.AddValue ("num_keys_node", "Number P of keys that form the key pool", num_keys_node);
    cmd.Parse (argc, argv);



    ///////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //                  FASE DE PRE DISTRIBUCIÓN DE LLAVES                    //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

    NS_LOG_INFO("*********KEY PRE DISTRIBUTION PHASE*****************");
    //Se genera el pool de llaves
    NS_LOG_INFO("GENERATING POOL.");
    pool = generatePool(pool_size);
    /*
    Codigo para imprimir todas las llaves, se deja comentado porque pueden ser muchas llaves
    for (size_t j = 0; j < pool_size; j++)
          {
            NS_LOG_INFO(j<<": "<<pool[j]);
          }
    */

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
    NS_LOG_INFO("********* END OF KEY PRE DISTRIBUTION PHASE*****************");
   /////////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //                          DESPLIEGUE DE LA RED                          //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  NS_LOG_INFO("********* KEY DISCOVERY PHASE*****************");
    //Creacion de la red
    NS_LOG_INFO("CREATING NODES.");
    //NodeContainer nodeContainer;
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
    //NetDeviceContainer mainDeviceContainer = wifi.Install (wifiPhy, mac, nodeContainer);
    mainDeviceContainer = wifi.Install (wifiPhy, mac, nodeContainer);

    //Intalacion de pila de protocolos a los dispositivos de red
    InternetStackHelper internet;
    internet.Install (nodeContainer);
    
    //Asignacion de direcciones ip
    NS_LOG_INFO ("MAIN:setting up ip adresses on all nodes");
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
    //cada dispositivo de red tendra direcciones 192.168.0.1, 192.168.0.2....192.168.0.254
    addressContainer = ipAddrs.Assign (mainDeviceContainer);
    
    
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
  std::vector<Ptr<Socket>> listening_sockets;
  //Se ponen a escuchar a todos los nodos
  for (uint32_t i = 0; i < nodeContainer.GetN (); ++i)
  {
    Ptr<Socket> destiny = Socket::CreateSocket (nodeContainer.Get (i), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
    destiny->Bind (local);
    destiny->SetRecvCallback (MakeCallback (&checkSharedKey));
    listening_sockets.push_back(destiny);
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
    simTime = i;
    Simulator::Schedule (Seconds (i),&SendStuff, source, remote, codedKeyIds);
  }
  simTime += 1;
  std::cout<<simTime<<std::endl;
  Simulator::Schedule (Seconds (simTime),&discoveryPhaseResults);

 /////////////////////////////////////////////////////////////////////////////
  //                                                                         //
  //             Pruebas entre nodos con enlaces directos                   //
  //                                                                       //
  ///////////////////////////////////////////////////////////////////////////

  
  simTime += 0.1;
  
  Simulator::Schedule (Seconds (simTime),&updateListeningFUnction, listening_sockets);
  for (size_t i = 0; i < numPackets; i++)
  {
    simTime += 1;
    Simulator::Schedule (Seconds (simTime),&sendSecure);
  }
  
  
  

  Simulator::Run ();

  Simulator::Stop (Seconds (30));

  AnimationInterface pAnim ("testProyecto.xml");


  
  Simulator::Destroy ();


  return 0;
  }
