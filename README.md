# Peer-to-Peer Chat Application over Wireless Embedded Nodes

## Project Summary

This project implements a peer-to-peer (P2P) wireless chat and data storage system using the TI CC1350 microcontroller and PicOS. Each device operates as an independent node capable of discovering nearby nodes in the same group, storing and retrieving short messages, and communicating wirelessly in a decentralized, distributed network.

The application demonstrates embedded system programming, low-level wireless protocol design, and UART-based user interfaces.

## Contributors
 - Adam Kearsey
 - Ryan-Jay Rosales
 - Kody Houle
 - Darion Kwasnitza

## Hardware and Tools

- Microcontroller: Texas Instruments CC1350
- Operating System: PicOS
- Communication Interface: UART (9600 baud)
- Network Stack: Custom message-passing protocol over wireless links

## Core Features

- Node Discovery: Nodes send discovery requests and gather responses to identify neighbors within the same group.
- P2P Messaging: Nodes can store messages on remote peers, retrieve them, and delete them by record index.
- Local Database: Each node maintains a local record database (up to 40 entries), storing metadata including sender ID and timestamp.
- UART Interface: A user can interact with the node using serial commands via a terminal interface.

## Message Protocols

All wireless messages follow a fixed-length packet format with a 250-byte maximum, including:

- Discovery Request and Response
- Record Creation
- Record Deletion
- Record Retrieval
- Response Messages with status codes and optional payloads

Each message includes:

- Group ID
- Message Type
- Request Number
- Sender and Receiver IDs
- Optional fields (record index, record string, status codes)

## UART Menu Commands

Each node responds to the following inputs from the connected computer:  

G - Set Group ID
N - Set Node ID
F - Find Neighbors
C - Create Record on Neighbor
D - Delete Record on Neighbor
R - Retrieve Record from Neighbor
S - Show Local Records
E - Erase Local Storage 


 


## Application Behavior

On startup, the node displays: 

- `Find (F)`: Sends discovery requests, waits for responses, and lists reachable node IDs.
- `Create (C)`: Asks user for target node ID and message, then transmits it to the peer.
- `Delete (D)`: Requests a node ID and record index to delete a message remotely.
- `Retrieve (R)`: Requests a node ID and index, then retrieves and displays the record.
- `Show (S)`: Displays all local records with timestamps and origin metadata.
- `Erase (E)`: Clears the local database.

## System Constraints

- Maximum of 40 stored messages per node
- Each message is up to 20 bytes (including null terminator)
- Record metadata includes sender ID and timestamp
- Nodes can only interact with others sharing the same Group ID

## Deliverables

- Complete working PicOS application
- UART menu-based command interface
- Wireless communication and message handling logic
- Demo-ready build flashed onto multiple CC1350 nodes


## Notes

- Communication between nodes is decentralized and peer-based.
- The application simulates a basic chat system between embedded devices without relying on a centralized server.


