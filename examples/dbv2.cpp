// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

// An example Dirbuster tool that uses Mathilda
// g++ -o dbv2 dbv2.cpp mathilda.cpp mathilda_utils.cpp mathilda_fork.cpp dirbuster.cpp -std=c++11 -ggdb -lcurl -luv
#include "mathilda.h"
#include "mathilda_utils.h"
#include "dirbuster.h"
#include <sstream>
#include <fstream>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {
    const char *h = argv[1];
    const char *p = argv[2];
    const char *d = argv[3];

    if(!h || !p || !d) {
        cout << "dbv2 <hosts> <pages> <directories>" << endl;
        return ERR;
    }

    std::vector <std::string> ips;
    std::vector <std::string> hosts;
    std::vector <std::string> tls_hosts;
    std::vector <std::string> pages;
    std::vector <std::string> dirs;

    MathildaUtils::read_file((char *) h, ips);
    MathildaUtils::read_file((char *) p, pages);
    MathildaUtils::read_file((char *) d, dirs);

    for(auto y : ips) {
        std::vector<std::string> out;
        // Expects format of "port:IP"
        MathildaUtils::split(y, ':', out);

        std::string host = out[1];
        MathildaUtils::addr_to_name(out[1], host);

        if(out[0] == "443") {
            tls_hosts.push_back(host);
        } else {
            hosts.push_back(host);
        }
    }

    auto cookie_file = "";
    Dirbuster *dirb = new Dirbuster(hosts, pages, dirs, cookie_file, 80);
    dirb->run();

    for(auto pt : dirb->paths) {
        cout << pt << endl;
    }

    delete dirb;

    dirb = new Dirbuster(tls_hosts, pages, dirs, cookie_file, 443);
    dirb->run();

    for(auto pt : dirb->paths) {
        cout << pt << endl;
    }

    delete dirb;
    return OK;
}