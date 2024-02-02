package main

import (
	"fmt"
	"net"
	"io"
)

const MAX_MESSAGE_LENGTH = 64

type Message struct {
	clientId string
	data string
}

// https://stackoverflow.com/questions/36417199/how-to-broadcast-message-using-channel/49877632#49877632
type Broker struct {
	stopCh    chan struct{}
	publishCh chan Message
	subCh     chan chan Message
	unsubCh   chan chan Message
}

func NewBroker() *Broker {
	return &Broker{
		stopCh:    make(chan struct{}),
		publishCh: make(chan Message, 1),
		subCh:     make(chan chan Message, 1),
		unsubCh:   make(chan chan Message, 1),
	}
}

func (b *Broker) Start() {
	subs := map[chan Message]struct{}{}
	for {
		select {
		case <-b.stopCh:
			return
		case msgCh := <-b.subCh:
			subs[msgCh] = struct{}{}
		case msgCh := <-b.unsubCh:
			delete(subs, msgCh)
		case msg := <-b.publishCh:
			for msgCh := range subs {
				select {
				// non-blocking send
				// https://gobyexample.com/non-blocking-channel-operations
				case msgCh <- msg:
				default:
				}
			}
		}
	}
}

func (b *Broker) Stop() {
	close(b.stopCh)
}

func (b *Broker) Subscribe() chan Message {
	msgCh := make(chan Message, 5)
	b.subCh <- msgCh
	return msgCh
}

func (b *Broker) Unsubscribe(msgCh chan Message) {
	b.unsubCh <- msgCh
}

func (b *Broker) Publish(msg Message) {
	b.publishCh <- msg
}

func handleWrite(conn net.Conn, outputCh chan Message, closeCh chan struct{}) {
	clientId := conn.RemoteAddr().String()

	for {
		select {
		case msg := <- outputCh:
			if (msg.clientId != clientId) {
				n, err := conn.Write([]byte(msg.data))
				if (n != len(msg.data)) {
					fmt.Printf("[ERROR] Can't fully write to %v: %v/%v\n", clientId, n, len(msg.data))
				} else if (err != nil) {
					fmt.Printf("[ERROR] Can't write to %v: %v\n", clientId, err)
					return
				}
			}

		case <-closeCh:
			fmt.Printf("[INFO] Close %v write handler\n", clientId)
			return
		}
	}
}

func handleRead(conn net.Conn, inputCh chan string, closeCh chan struct{}) {
	clientId := conn.RemoteAddr().String()
	buf := make([]byte, MAX_MESSAGE_LENGTH + 1)

	defer close(closeCh)

	for {
		n, err := conn.Read(buf)
		if (err != nil) {
			if (err == io.EOF) {
				fmt.Printf("[INFO] EOF from %v\n", clientId)
				return
			} else {
				fmt.Printf("[ERROR] Can't read from %v: %v\n", clientId, err)
			}
		} else if (n > MAX_MESSAGE_LENGTH) {
			fmt.Printf("[INFO] Invalid message from %v\n", clientId)
			return
		} else {
			inputCh <-string(buf[0:n])
		}
	}
}

func handleConnection(conn net.Conn, br *Broker) {
	clientId := conn.RemoteAddr().String()
	fmt.Printf("[INFO] New connection %v\n", clientId)

	inputCh := make(chan string)
	// https://www.programming-books.io/essential/go/signaling-channel-with-chan-struct-f5daa999f5134b9ba9f2d69916df292a
	closeCh := make(chan struct{})
	outputCh := br.Subscribe()

	defer func() {
		conn.Close()
		br.Unsubscribe(outputCh)
		fmt.Printf("[INFO] Client %v disconnected\n", clientId)
	}()

	go handleRead(conn, inputCh, closeCh)
	go handleWrite(conn, outputCh, closeCh)

	for {
		select {
		case msg := <-inputCh:
			fmt.Printf("[INFO] Message from %v: %v", clientId, msg)
			// use pointer???
			br.Publish(Message{
				data: msg,
				clientId: clientId,
			})
		case <-closeCh:
			return
		}
	}
}

func main() {
	addr := "0.0.0.0:9001"
	ln, err := net.Listen("tcp", addr)
	if (err != nil) {
		fmt.Printf("[ERROR] Can't listen: %v", err)
	}

	fmt.Printf("[INFO] Listen on %v\n", addr)

	br := NewBroker()
	go br.Start()

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Printf("[ERROR] Can't accept: %v", err)
		}

		go handleConnection(conn, br)
	}
}
