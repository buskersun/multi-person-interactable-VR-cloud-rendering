/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
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
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/tcp-socket-factory.h"
#include "tcp-stream-server.h"
#include "ns3/global-value.h"
#include <ns3/core-module.h>
#include "tcp-stream-client.h"
#include "ns3/trace-source-accessor.h"
#include <iterator>
#include <numeric>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <stdexcept>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpStreamServerApplication");

NS_OBJECT_ENSURE_REGISTERED (TcpStreamServer);

template <typename T> std::string ToString (T val)
{
  std::stringstream stream;
  stream << val;
  return stream.str ();
}

TypeId TcpStreamServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpStreamServer")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<TcpStreamServer> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&TcpStreamServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("SegmentDuration",
                   "The duration of a segment in nanoseconds",
                   UintegerValue (2000000),
                   MakeUintegerAccessor (&TcpStreamServer::m_segmentDuration),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("SegmentSizeFilePath",
                   "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes",
                   StringValue ("bitrates.txt"),
                   MakeStringAccessor (&TcpStreamServer::m_segmentSizeFilePath),
                   MakeStringChecker ())
    .AddAttribute ("Chunk",
                   "The number of chunks in a segment, use 0 if segments have no chunks",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpStreamServer::chunk),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Cmaf",
                   "Cmaf version",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpStreamServer::cmaf),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

TcpStreamServer::TcpStreamServer ()
{
  NS_LOG_FUNCTION (this);
}

TcpStreamServer::~TcpStreamServer ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
}

void TcpStreamServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void TcpStreamServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      m_socket->Listen ();
    }

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      m_socket6->Bind (local6);
      m_socket->Listen ();
    }

  // Accept connection requests from remote hosts.
  m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr< Socket >, const Address &> (),
                               MakeCallback (&TcpStreamServer::HandleAccept,this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&TcpStreamServer::HandlePeerClose, this),
    MakeCallback (&TcpStreamServer::HandlePeerError, this));

  // read segment/chunk sizes
  std::ifstream myfile;
  myfile.open (ToString (m_segmentSizeFilePath).c_str ());
  if (!myfile) 
  {
    //std::cerr << "SERVER: Segmnent size file is missing!" << "\n";
  }
  std::string temp;
  while (std::getline (myfile, temp))
  {
    if (temp.empty ())
    {
      break;
    }
    std::istringstream buffer (temp);
    std::vector<int64_t> line ((std::istream_iterator<int64_t> (buffer)), std::istream_iterator<int64_t>());
    line.erase(line.begin());
    segmentSize.push_back (line);
  }
  NS_ASSERT_MSG (!segmentSize.empty (), "No segment sizes read from file.");  
}

void TcpStreamServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_socket6 != 0)
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void TcpStreamServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  packet = socket->RecvFrom (from);
  int64_t quality = GetCommand (packet);

  int64_t packetSizeToReturn = segmentSize.at (quality).at (m_callbackData [from].m_segmentCounter);
  m_callbackData [from].currentTxBytes = 0;
  m_callbackData [from].packetSizeToReturn = packetSizeToReturn;
  m_callbackData [from].send = true;

  HandleSend (socket, socket->GetTxAvailable ());


  m_callbackData [from].m_segmentCounter += 1;

  if(chunk > 0) { //CMAF & CTE

    int currentChunk = m_callbackData [from].m_segmentCounter;
    for(int i=1; i<chunk; i++) {

      double availabilityTime = (m_segmentDuration*(currentChunk+i-4));
      Time delay = MicroSeconds(availabilityTime-Simulator::Now ().GetMicroSeconds ());

      if(delay <= 0) {

        packetSizeToReturn = segmentSize.at (quality).at (m_callbackData [from].m_segmentCounter);
        // these values will be accessible by the clients Address from.
        m_callbackData [from].currentTxBytes = 0;
        m_callbackData [from].packetSizeToReturn = packetSizeToReturn;
        m_callbackData [from].send = true;

        HandleSend (socket, socket->GetTxAvailable ());

        m_callbackData [from].m_segmentCounter += 1;


      } else {

        Simulator::Schedule (delay, &TcpStreamServer::scheduleChunk, this, socket, from, quality);

      }

    }

  }
}

void TcpStreamServer::scheduleChunk(Ptr<Socket> socket, Address from, int64_t quality) {

  int64_t packetSizeToReturn = segmentSize.at (quality).at (m_callbackData [from].m_segmentCounter);
 // these values will be accessible by the clients Address from.
  m_callbackData [from].currentTxBytes = 0;
  m_callbackData [from].packetSizeToReturn = packetSizeToReturn;
  m_callbackData [from].send = true;

  HandleSend (socket, socket->GetTxAvailable ());

  m_callbackData [from].m_segmentCounter += 1;

}

void TcpStreamServer::HandleSend (Ptr<Socket> socket, uint32_t txSpace)
{
  Address from;
  socket->GetPeerName (from);
  // look up values for the connected client and whose values are stored in from
  if (m_callbackData [from].currentTxBytes == m_callbackData [from].packetSizeToReturn)
    {
      m_callbackData [from].currentTxBytes = 0;
      m_callbackData [from].packetSizeToReturn = 0;
      m_callbackData [from].send = false;
      return;
    }
  if (socket->GetTxAvailable () > 0 && m_callbackData [from].send)
    {
      int32_t toSend;
      toSend = std::min (socket->GetTxAvailable (), m_callbackData [from].packetSizeToReturn - m_callbackData [from].currentTxBytes);
      Ptr<Packet> packet = Create<Packet> (toSend);
      int amountSent = socket->Send (packet, 0);
      if (amountSent > 0)
        {
          m_callbackData [from].currentTxBytes += amountSent;
        }
      // We exit this part, when no bytes have been sent, as the send side buffer is full.
      // The "HandleSend" callback will fire when some buffer space has freed up.
      else
        {
          return;
        }
    }
}

void TcpStreamServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  callbackData cbd;
  cbd.currentTxBytes = 0;
  cbd.packetSizeToReturn = 0;
  cbd.send = false;
  cbd.m_segmentCounter = 0;
  m_callbackData [from] = cbd;
  m_connectedClients.push_back (from);
  s->SetRecvCallback (MakeCallback (&TcpStreamServer::HandleRead, this));
  s->SetSendCallback ( MakeCallback (&TcpStreamServer::HandleSend, this));
}

void TcpStreamServer::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address from;
  socket->GetPeerName (from);
  for (std::vector<Address>::iterator it = m_connectedClients.begin (); it != m_connectedClients.end (); ++it)
    {
      if (*it == from)
        {
          m_connectedClients.erase (it);
          // No more clients left in m_connectedClients, simulation is done.
          if (m_connectedClients.size () == 0)
            {
              Simulator::Stop ();
            }
          return;
        }
    }
}

void TcpStreamServer::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

int64_t TcpStreamServer::GetCommand (Ptr<Packet> packet)
{
  int64_t packetSizeToReturn;
  uint8_t *buffer = new uint8_t [packet->GetSize ()];
  packet->CopyData (buffer, packet->GetSize ());
  std::stringstream ss;
  ss << buffer;
  std::string str;
  ss >> str;
  std::stringstream convert (str);
  convert >> packetSizeToReturn;
  return packetSizeToReturn;
}
} // Namespace ns3
