# simple-chat-room

A simple chat room using 'queue' and 'select' with custom protocol. A server is running and many clients connected to it. Each client choose a nickname and other connected ones will be informed. Saving log in each side is optional.

## Protocol

Simple and custom protocol:

| Header      | Header      |Body        |
| ----------- | ----------- |----------- |
| type        | length      | body       |
| 2 bytes     | 2 bytes     |65532 bytes |


Just 3 types are supported: (1) joining, (2) message, (3) broadcast

type-1 is used to send join to inform others
type-2 is used to send message
type-3 is used for sending nickname of joined client to all other connected ones.

##Install:

make

##Usage:
run server: ./scr -l -i 127.0.0.1 -p 12000

run client1: ./scr -i 127.0.0.1 -p 12000 -n user1 -f log-user1 -v
