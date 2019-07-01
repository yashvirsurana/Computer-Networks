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

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"

#include <string>
#include <stdio.h>
#include <ctime>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AssignmentTemplate");

double
FlowOutput(Ptr<FlowMonitor> flowmon, FlowMonitorHelper *flowmonHelper)
{
  flowmon->CheckForLostPackets (); //check all packets have been sent or completely lost
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper->GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats (); // pull stats from flow monitor
  double aggregateThru = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
  {//iterate through collected flow stats and find the traffic from node 1 to node 0
    //Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
    //if (t.sourceAddress == Ipv4Address("10.1.1.1") && t.destinationAddress == Ipv4Address("10.1.1.2")) //use the node addresses to select the flow (or flows) you want
    //{
    //  NS_LOG_UNCOND("Throughput in Kib/s over the run time: " //normal c++ output streams can be used to output this information to a file in place of the terminal
    //          << iter->second.rxBytes * 8.0 / (10 * 1024) //bits per byte/run time over bits per Kib
    //          << std::endl); //flowmonitor records a number of different statistics about each flow, however we are only interested in rxBytes
    //}
    aggregateThru += iter->second.rxBytes * 8.0 / (10 * 1024) ;
    //printf("%f\n",aggregateThru);

  }
  return aggregateThru;
}

double 
part1 (bool fading, int method, int seedz, int dist_step) // '0 for Aara, 1 for Cara'
{
  bool verbose = false;
  bool rayleigh = fading;

  int numOfStaNodes = 1;

  if (verbose)
    {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO); //packet sink should write all actions to the command line
    }

  SeedManager::SetSeed (seedz);//seed number should change between runs if running multiple simulations.

  NodeContainer wifiStaNodes; //create AP Node and (one or more) Station node(s)
  wifiStaNodes.Create (numOfStaNodes);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel; //create helpers for the channel and phy layer and set propagation configuration here.
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  if(rayleigh){
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel",
	                           "m0", DoubleValue(1.0), "m1", DoubleValue(1.0), "m2", DoubleValue(1.0)); //combining log and nakagami to have both distance and rayleigh fading (nakami with m0, m1 and m2 =1 is rayleigh)
  }

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default (); //create helper for the overall wifi setup and configure a station manager
  wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211g);
  if (method==0) {
  	wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  }
  else {
  	wifi.SetRemoteStationManager ("ns3::CaraWifiManager");
  }

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default (); //create a mac helper and configure for station and AP and install

  Ssid ssid = Ssid ("example-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator", //places nodes in a grid with distance between nodes and grid width defined below, for 2 nodes can be used to easily place them at a set distance apart
                                 "MinX", DoubleValue (0.0), //first node positioned at x = 0, y= 0
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (dist_step*5.0), //nodes will be place 5m apart in x plane
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3), 
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); //nodes shouldn't move once placed
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack; //install the internet stack on both nodes
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address; //assign IP addresses to all nodes
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (staDevices);
  Ipv4InterfaceContainer apAddress = address.Assign (apDevices); //we need to keep AP address accessible as we need it later
  Ipv4InterfaceContainer stnAddress = address.Assign (staDevices);

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address ()); //create a new on-off application to send data
  std::string dataRate = "20Mib/s"; //data rate set as a string, see documentation for accepted units
  onoff.SetConstantRate(dataRate, (uint32_t)1024); //set the onoff client application to CBR mode

  // Gnerate random boolean to decide which is sender which is receiver.

  for (int i = 0; i < numOfStaNodes; i++) {

	  srand(time(NULL));
	  //int a=rand()%2;
	  int a = 1;
	  if (a == 0) { // AP sender 

	  AddressValue remoteAddress (InetSocketAddress (stnAddress.GetAddress (i), 8000)); //specify address and port of the AP as the destination for on-off application's packets

	  onoff.SetAttribute ("Remote", remoteAddress);

	  ApplicationContainer apps = onoff.Install (wifiApNode.Get (0));//install onoff application on stanode 0 and configure start/stop times


	  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
	  apps.Start(Seconds(var->GetValue(0, 0.1)));
	  apps.Stop (Seconds (10.0)); // SHOUDL BE 10

	  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress (stnAddress.GetAddress (i), 8000)); //create packet sink on AP node with address and port that on-off app is sending to
	  apps.Add(sink.Install(wifiStaNodes.Get(i)));

	  }
	  else { // STA sender

	  AddressValue remoteAddress (InetSocketAddress (apAddress.GetAddress (0), 8000)); //specify address and port of the AP as the destination for on-off application's packets

	  onoff.SetAttribute ("Remote", remoteAddress);

	  ApplicationContainer apps = onoff.Install (wifiStaNodes.Get (i));//install onoff application on stanode 0 and configure start/stop times


	  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
	  apps.Start(Seconds(var->GetValue(0, 0.1)));
	  apps.Stop (Seconds (10.0)); // should be 10

	  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress (apAddress.GetAddress (0), 8000)); //create packet sink on AP node with address and port that on-off app is sending to
	  apps.Add(sink.Install(wifiApNode.Get(0)));

	  }
	}

  Simulator::Stop (Seconds (10.0)); // should be 10//define stop time of simulator

  Ptr<FlowMonitor> flowmon; //create an install a flow monitor to monitor all transmissions around the network
  FlowMonitorHelper flowmonHelper;// = (FlowMonitorHelper*)malloc(sizeof(flowmonHelper));
  flowmon = flowmonHelper.InstallAll();

  Simulator::Run (); //run the simulation and destroy it once done
  Simulator::Destroy ();
  double a = FlowOutput(flowmon, &flowmonHelper);

  //printf("%f\n", a);
  return a;
}

int
main (int argc, char *argv[]) 
{
  // AARF _ NO FADING
  printf("AARF_NO_FADING\n");
  // DISTANCES 5 to 100 (in 20 steps)
  for (int z = 1; z < 21; z++) { 

  	printf("Distance: %d\t",z*5);
  	double averageThru = 0;
  
  // Average of 5
  	for (int y = 0; y <5; y++) {
    	averageThru += part1 (false, 0, (y+1)*2, z); // part1 (bool fading, int method, int seedz, int dist_step)
  	}
  	averageThru = averageThru/5.0;

  printf("%f\n",averageThru);
  }

  // AARF _ RAYLEIGH_FADING
  printf("AARF_Rayleigh\n");
  // DISTANCES 5 to 100 (in 20 steps)
  for (int z = 1; z < 21; z++) { 

  	printf("Distance: %d\t",z*5);
  	double averageThru = 0;
  
  // Average of 5
  	for (int y = 0; y <5; y++) {
    	averageThru += part1 (true, 0, (y+1)*2, z); // part1 (bool fading, int method, int seedz, int dist_step)
  	}
  	averageThru = averageThru/5.0;

  printf("%f\n",averageThru);
  }

  // CARA _NO_FADING
  printf("CARA_NO_FADING\n");
  // DISTANCES 5 to 100 (in 20 steps)
  for (int z = 1; z < 21; z++) { 

  	printf("Distance: %d\t",z*5);
  	double averageThru = 0;
  
  // Average of 5
  	for (int y = 0; y <5; y++) {
    	averageThru += part1 (false, 1, (y+1)*2, z); // part1 (bool fading, int method, int seedz, int dist_step)
  	}
  	averageThru = averageThru/5.0;

  printf("%f\n",averageThru);
  }

  // CARA _ RAYLEIGH_FADING
  printf("CARA_Rayleigh\n");
  // DISTANCES 5 to 100 (in 20 steps)
  for (int z = 1; z < 21; z++) { 

  	printf("Distance: %d\t",z*5);
  	double averageThru = 0;
  
  // Average of 5
  	for (int y = 0; y <5; y++) {
    	averageThru += part1 (true, 1, (y+1)*2, z); // part1 (bool fading, int method, int seedz, int dist_step)
  	}
  	averageThru = averageThru/5.0;

  printf("%f\n",averageThru);
  }





  return 0;
}

