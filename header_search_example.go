// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
package main

import (
	"fmt"
	"net/http"
	"sync"
)

const (
	Threads = 256
)

var Hosts = []string{"10.0.0.1", "10.0.0.2", "10.0.0.3"}

func main() {
	wg := new(sync.WaitGroup)
	hostChan := make(chan string, Threads*2)
	for i := 0; i < Threads; i++ {
		wg.Add(1)
		go worker(wg, hostChan)
	}

	for _, host := range Hosts {
		hostChan <- host
	}
	close(hostChan)

	wg.Wait()
	fmt.Println("All done")
}

func worker(wg *sync.WaitGroup, hostChan chan string) {
	defer wg.Done()

	client := http.Client{}

	search := http.CanonicalHeaderKey("x-forwarded-for")
	for host := range hostChan {
		res, err := client.Get(host)
		if err != nil {
			continue
		}

		headers := res.Header.Get(search)
		if len(headers) != 0 {
			fmt.Println("Found on host", host, headers)
		}
	}
}
