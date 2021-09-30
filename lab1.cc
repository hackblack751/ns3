/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Change these following parameters for different experiments
  //
  double lat = 2.0; // Latency (in miliseconds)
  uint64_t rate = 5000000; // Data rate in bps 
  double interval = 0.05; // Waiting time

  // Visualization
  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("interval", "UDP client packet interval", interval);
  cmd.Parse (argc, argv);

  // Create a NodeContainer containing 3 nodes
  NodeContainer nodes;
  nodes.Create (3);
  
  // Create and initialize a PointToPoint channel
  PointToPointHelper p2p;
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (lat)));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  // Create a connection between node0 and node1 via a NetDeviceContainer
  NetDeviceContainer dev = p2p.Install (nodes.Get(0), nodes.Get(1));
  // Create a similar one for node1 and node2
  NetDeviceContainer dev2 = p2p.Install (nodes.Get(1), nodes.Get(2));


//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//


  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes); 

  Ipv4AddressHelper ipv4; // đĩa chỉ ipv4

  // Allocate the network 10.1.1.0/24 to the dev which is the NetDeviceContainer connecting nodeo to node1
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (dev);
  // Similarly repeat for the dev2 with the network 10.1.2.0/24 
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (dev2);

  // Enable routing 
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//
// Create one udpServer application on node 2.
//
  uint16_t port1 = 8000; 
  uint16_t port2 = 8800;  
  // Creat a UdpServer running on port 8000 and 8800
  UdpServerHelper server1 (port1);
  UdpServerHelper server2 (port2);
  // Install the UdpServer on node2
  ApplicationContainer apps;
  apps = server1.Install (nodes.Get (2));
  apps = server2.Install(nodes.Get(2));
   
  // Specify the experimental time
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));


 
 

//
// Create one udpClient application on node 0.
//

  // Create a UdpClient
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (interval);
  uint32_t maxPacketCount = 320; 

  UdpClientHelper client1 (i2.GetAddress (1), port1);
  UdpClientHelper client2 (i2.GetAddress (1), port2);
  // Set up parameters for the UdpClient client1
  client1.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client1.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client1.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  //Set up parameters for the UdpClient client2
  client2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  // Install the UdpClient on node0
  apps = client1.Install (nodes.Get (0));
  apps = client2.Install (nodes.Get (0));
  
  // Specify the experimental time
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

 
 


//
// Enable dev in NetDeviceContainer to make a trace
//
  AsciiTraceHelper ascii;
  p2p.EnableAscii(ascii.CreateFileStream ("lab-1-dev.tr"), dev);
  p2p.EnablePcap("lab-1-dev", dev, false);
  p2p.EnableAscii(ascii.CreateFileStream ("lab-1-dev2.tr"), dev2);
  p2p.EnablePcap("lab-1-dev2", dev2, false);

  // Monitor the traffic flow
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  // Begin the simulation
  Simulator::Stop (Seconds(11.0));
  Simulator::Run ();

  // Log monitoring information
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
     }

  // Export FlowMonitor to a file
  monitor->SerializeToXmlFile("lab-1.flowmon", true, true);
  // Destroy the simulator
  Simulator::Destroy ();
}
