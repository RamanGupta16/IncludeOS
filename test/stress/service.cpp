// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os>
#include <net/inet4>
#include <net/dhcp/dh4client.hpp>
#include <math.h> // rand()
#include <sstream>

// An IP-stack object
std::unique_ptr<net::Inet4<VirtioNet> > inet;

using namespace std::chrono;


std::string header(int content_size) {
  std::string head="HTTP/1.1 200 OK \n "                \
    "Date: Mon, 01 Jan 1970 00:00:01 GMT \n"            \
    "Server: IncludeOS prototype 4.0 \n"                \
    "Last-Modified: Wed, 08 Jan 2003 23:11:55 GMT \n"     \
    "Content-Type: text/html; charset=UTF-8 \n"            \
    "Content-Length: "+std::to_string(content_size)+"\n"   \
    "Accept-Ranges: bytes\n"                               \
    "Connection: close\n\n";

  return head;
}

std::string html() {
  int color = rand();
  std::stringstream stream;

  /* HTML Fonts */
  std::string ubuntu_medium  = "font-family: \'Ubuntu\', sans-serif; font-weight: 500; ";
  std::string ubuntu_normal  = "font-family: \'Ubuntu\', sans-serif; font-weight: 400; ";
  std::string ubuntu_light  = "font-family: \'Ubuntu\', sans-serif; font-weight: 300; ";

  /* HTML */
  stream << "<html><head>"
         << "<link href='https://fonts.googleapis.com/css?family=Ubuntu:500,300' rel='stylesheet' type='text/css'>"
         << "</head><body>"
         << "<h1 style= \"color: " << "#" << std::hex << (color >> 8) << "\">"
         <<  "<span style=\""+ubuntu_medium+"\">Include</span><span style=\""+ubuntu_light+"\">OS</span> </h1>"
         <<  "<h2>Now speaks TCP!</h2>"
    // .... generate more dynamic content
         << "<p> This is improvised http, but proper suff is in the works. </p>"
         << "<footer><hr /> &copy; 2016, IncludeOS AS @ 60&deg; north </footer>"
         << "</body></html>\n";

  std::string html = stream.str();

  return html;
}

const std::string NOT_FOUND = "HTTP/1.1 404 Not Found \n Connection: close\n\n";

extern char _end;

uint64_t TCP_BYTES_RECV = 0;
uint64_t TCP_BYTES_SENT = 0;

void Service::start() {
  // Assign a driver (VirtioNet) to a network interface (eth0)
  // @note: We could determine the appropirate driver dynamically, but then we'd
  // have to include all the drivers into the image, which  we want to avoid.
  hw::Nic<VirtioNet>& eth0 = hw::Dev::eth<0,VirtioNet>();

  // Bring up a network stack, attached to the nic
  // @note : No parameters after 'nic' means we'll use DHCP for IP config.
  inet = std::make_unique<net::Inet4<VirtioNet> >(eth0);

  // Static IP configuration, until we (possibly) get DHCP
  // @note : Mostly to get a robust demo service that it works with and without DHCP
  inet->network_config( { 10,0,0,42 },      // IP
                        { 255,255,255,0 },  // Netmask
                        { 10,0,0,1 },       // Gateway
                        { 8,8,8,8 } );      // DNS

  srand(OS::cycles_since_boot());

  // Set up a TCP server
  auto& server = inet->tcp().bind(80);
  inet->tcp().set_MSL(5s);
  auto& server_mem = inet->tcp().bind(4243);

  // Set up a UDP server
  net::UDP::port_t port = 4242;
  auto& conn = inet->udp().bind(port);

  net::UDP::port_t port_mem = 4243;
  auto& conn_mem = inet->udp().bind(port_mem);



  hw::PIT::instance().onRepeatedTimeout(10s, []{
      printf("<Service> TCP STATUS:\n%s \n", inet->tcp().status().c_str());
      auto memuse =  OS::memory_usage();
      printf("Current memory usage: %i b, (%f MB) \n", memuse, float(memuse)  / 1000000);
      printf("Recv: %llu Sent: %llu\n", TCP_BYTES_RECV, TCP_BYTES_SENT);

    });

  server_mem.onConnect([] (auto conn) {
      conn->read(1024, [conn](net::TCP::buffer_t buf, size_t n) {
          TCP_BYTES_RECV += n;
          // create string from buffer
          std::string received { (char*)buf.get(), n };
          auto reply = std::to_string(OS::memory_usage())+"\n";
          // Send the first packet, and then wait for ARP
          printf("TCP Mem: Reporting memory size as %s bytes\n", reply.c_str());
          conn->write(reply.c_str(), reply.size(), [conn](size_t n) {
              TCP_BYTES_SENT += n;
            });

          conn->onDisconnect([](auto c, auto){
              c->close();
            });
        });
    });



  // Add a TCP connection handler - here a hardcoded HTTP-service
  server.onConnect([] (auto conn) {
        // read async with a buffer size of 1024 bytes
        // define what to do when data is read
        conn->read(1024, [conn](net::TCP::buffer_t buf, size_t n) {
            TCP_BYTES_RECV += n;
            // create string from buffer
            std::string data { (char*)buf.get(), n };

            if (data.find("GET / ") != std::string::npos) {

              auto htm = html();
              auto hdr = header(htm.size());

              // create response
              conn->write(hdr.data(), hdr.size(), [](size_t n) { TCP_BYTES_SENT += n; });
              conn->write(htm.data(), htm.size(), [](size_t n) { TCP_BYTES_SENT += n; });
            }
            else {
              conn->write(NOT_FOUND.data(), NOT_FOUND.size(), [](size_t n) { TCP_BYTES_SENT += n; });
            }
          });

      }).onDisconnect([](auto conn, auto reason) {
          conn->close();
        }).onPacketReceived([](auto, auto packet) {});


  // UDP connection handler
  conn.on_read([&] (net::UDP::addr_t addr, net::UDP::port_t port, const char* data, int len) {
      std::string received = std::string(data,len);
      std::string reply = received;

      // Send the first packet, and then wait for ARP
      conn.sendto(addr, port, reply.c_str(), reply.size());
    });

  // UDP utility to return memory usage
  conn_mem.on_read([&] (net::UDP::addr_t addr, net::UDP::port_t port, const char* data, int len) {
      std::string received = std::string(data,len);
      Expects(received == "memsize");
      auto reply = std::to_string(OS::memory_usage());
      // Send the first packet, and then wait for ARP
      printf("Reporting memory size as %s bytes\n", reply.c_str());
      conn.sendto(addr, port, reply.c_str(), reply.size());
    });



  printf("*** TEST SERVICE STARTED *** \n");
  auto memuse = OS::memory_usage();
  printf("Current memory usage: %i b, (%f MB) \n", memuse, float(memuse)  / 1000000);

  /** These printouts are event-triggers for the vmrunner **/
  printf("Ready to start\n");
  printf("Ready for ARP\n");
  printf("Ready for UDP\n");
  printf("Ready for ICMP\n");
  printf("Ready for TCP\n");
  printf("Ready to end\n");
}
